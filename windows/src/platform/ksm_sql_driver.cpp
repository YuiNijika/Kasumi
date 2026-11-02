#include "ksm_sql_driver.h"

#include <algorithm>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kMaxSqlConnections = 32;
constexpr std::size_t kMaxSqlResultRows = 10000;
constexpr std::size_t kMaxSqlResultBytes = 16 * 1024 * 1024;

std::size_t SqlValueBytes(const KSMSqlValue& value) {
    return std::visit(
        [](const auto& item) -> std::size_t {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return item.size();
            }
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                return 0;
            }
            if constexpr (std::is_same_v<T, bool>) {
                return 1;
            }
            return sizeof(item);
        },
        value
    );
}

void ValidateSqlResult(const KSMSqlQueryResult& result) {
    if (result.rows.size() > kMaxSqlResultRows) {
        throw KSMHostException(
            "RESOURCE_LIMIT",
            "The SQL result exceeds the maximum row count."
        );
    }
    std::size_t bytes = 0;
    for (const auto& row : result.rows) {
        for (const auto& value : row) {
            bytes += SqlValueBytes(value);
            if (bytes > kMaxSqlResultBytes) {
                throw KSMHostException(
                    "RESOURCE_LIMIT",
                    "The SQL result exceeds the maximum payload size."
                );
            }
        }
    }
}

}  // namespace

class KSMSqlDriverRegistry {
public:
    void Register(std::shared_ptr<KSMSqlDriver> driver);
    std::vector<std::string> Drivers() const;
    std::optional<std::string> Open(
        const std::string& driver,
        const std::string& url
    );
    std::optional<KSMSqlQueryResult> Query(
        const std::string& connection_id,
        const std::string& statement,
        const std::vector<KSMSqlValue>& params
    );
    std::optional<KSMSqlExecuteResult> Execute(
        const std::string& connection_id,
        const std::string& statement,
        const std::vector<KSMSqlValue>& params
    );
    bool Close(const std::string& connection_id);

private:
    struct Connection {
        std::shared_ptr<KSMSqlDriver> driver;
        std::shared_ptr<KSMSqlConnection> value;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<KSMSqlDriver>> drivers_;
    std::unordered_map<std::string, Connection> connections_;
    std::size_t pending_connections_ = 0;
    std::uint64_t next_connection_id_ = 1;
};

void KSMSqlDriverRegistry::Register(
    std::shared_ptr<KSMSqlDriver> driver
) {
    if (!driver) {
        throw std::invalid_argument("A Kasumi SQL driver cannot be null.");
    }
    const std::string name = driver->Name();
    if (name.empty()) {
        throw std::invalid_argument("A Kasumi SQL driver must have a name.");
    }
    std::lock_guard lock(mutex_);
    if (drivers_.contains(name)) {
        throw std::invalid_argument("Kasumi SQL driver names must be unique.");
    }
    drivers_.emplace(name, std::move(driver));
}

std::vector<std::string> KSMSqlDriverRegistry::Drivers() const {
    std::lock_guard lock(mutex_);
    std::vector<std::string> names;
    names.reserve(drivers_.size());
    for (const auto& [name, driver] : drivers_) {
        (void)driver;
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::optional<std::string> KSMSqlDriverRegistry::Open(
    const std::string& driver,
    const std::string& url
) {
    std::shared_ptr<KSMSqlDriver> selected;
    {
        std::lock_guard lock(mutex_);
        const auto entry = drivers_.find(driver);
        if (entry != drivers_.end()) {
            selected = entry->second;
        }
    }
    if (!selected) {
        return std::nullopt;
    }
    {
        std::lock_guard lock(mutex_);
        if (
            connections_.size() + pending_connections_ >=
            kMaxSqlConnections
        ) {
            throw KSMHostException(
                "RESOURCE_LIMIT",
                "The SQL connection limit has been reached."
            );
        }
        ++pending_connections_;
    }
    std::shared_ptr<KSMSqlConnection> connection;
    try {
        connection = selected->Open(url);
    } catch (...) {
        std::lock_guard lock(mutex_);
        --pending_connections_;
        throw;
    }
    if (!connection) {
        std::lock_guard lock(mutex_);
        --pending_connections_;
        throw KSMHostException(
            "IO_ERROR",
            "The SQL driver did not create a connection."
        );
    }
    std::string connection_id;
    {
        std::lock_guard lock(mutex_);
        --pending_connections_;
        connection_id = "sql-" + std::to_string(next_connection_id_++);
        connections_.emplace(
            connection_id,
            Connection{std::move(selected), std::move(connection)}
        );
    }
    return connection_id;
}

std::optional<KSMSqlQueryResult> KSMSqlDriverRegistry::Query(
    const std::string& connection_id,
    const std::string& statement,
    const std::vector<KSMSqlValue>& params
) {
    std::shared_ptr<KSMSqlConnection> connection;
    {
        std::lock_guard lock(mutex_);
        const auto entry = connections_.find(connection_id);
        if (entry != connections_.end()) {
            connection = entry->second.value;
        }
    }
    if (!connection) {
        throw KSMHostException(
            "NOT_FOUND",
            "The SQL connection does not exist."
        );
    }
    auto result = connection->Query(statement, params);
    ValidateSqlResult(result);
    return result;
}

std::optional<KSMSqlExecuteResult> KSMSqlDriverRegistry::Execute(
    const std::string& connection_id,
    const std::string& statement,
    const std::vector<KSMSqlValue>& params
) {
    std::shared_ptr<KSMSqlConnection> connection;
    {
        std::lock_guard lock(mutex_);
        const auto entry = connections_.find(connection_id);
        if (entry != connections_.end()) {
            connection = entry->second.value;
        }
    }
    if (!connection) {
        throw KSMHostException(
            "NOT_FOUND",
            "The SQL connection does not exist."
        );
    }
    return connection->Execute(statement, params);
}

bool KSMSqlDriverRegistry::Close(const std::string& connection_id) {
    std::lock_guard lock(mutex_);
    return connections_.erase(connection_id) > 0;
}

KSMHostServices::KSMHostServices()
    : sql_drivers_(std::make_unique<KSMSqlDriverRegistry>()) {
}

KSMHostServices::~KSMHostServices() = default;

void KSMHostServices::RegisterSqlDriver(
    std::shared_ptr<KSMSqlDriver> driver
) {
    sql_drivers_->Register(std::move(driver));
}

std::vector<std::string> KSMHostServices::SqlDrivers() const {
    return sql_drivers_->Drivers();
}

std::optional<std::string> KSMHostServices::SqlOpen(
    const std::string& driver,
    const std::string& url
) {
    return sql_drivers_->Open(driver, url);
}

std::optional<KSMSqlQueryResult> KSMHostServices::SqlQuery(
    const std::string& connection_id,
    const std::string& statement,
    const std::vector<KSMSqlValue>& params
) {
    return sql_drivers_->Query(connection_id, statement, params);
}

std::optional<KSMSqlExecuteResult> KSMHostServices::SqlExecute(
    const std::string& connection_id,
    const std::string& statement,
    const std::vector<KSMSqlValue>& params
) {
    return sql_drivers_->Execute(connection_id, statement, params);
}

bool KSMHostServices::SqlClose(const std::string& connection_id) {
    return sql_drivers_->Close(connection_id);
}
