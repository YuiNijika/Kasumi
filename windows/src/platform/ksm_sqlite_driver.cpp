#include "ksm_sqlite_driver.h"

#include <sqlite3.h>
#include <windows.h>

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "../commands/ksm_filesystem.h"
#include "ksm_virtual_path.h"

namespace {

std::string_view LeadingStatement(std::string_view statement) {
    while (!statement.empty()) {
        while (!statement.empty() &&
            std::isspace(static_cast<unsigned char>(statement.front())) != 0) {
            statement.remove_prefix(1);
        }
        if (statement.starts_with("--")) {
            const std::size_t newline = statement.find('\n');
            statement.remove_prefix(
                newline == std::string_view::npos
                    ? statement.size()
                    : newline + 1
            );
            continue;
        }
        if (statement.starts_with("/*")) {
            const std::size_t end = statement.find("*/", 2);
            statement.remove_prefix(
                end == std::string_view::npos ? statement.size() : end + 2
            );
            continue;
        }
        break;
    }
    return statement;
}

bool StartsKeyword(std::string_view statement, std::string_view keyword) {
    if (statement.size() < keyword.size()) {
        return false;
    }
    for (std::size_t index = 0; index < keyword.size(); ++index) {
        if (
            std::tolower(static_cast<unsigned char>(statement[index])) !=
            std::tolower(static_cast<unsigned char>(keyword[index]))
        ) {
            return false;
        }
    }
    return statement.size() == keyword.size() ||
        std::isspace(static_cast<unsigned char>(statement[keyword.size()])) != 0;
}

std::string ToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        size,
        nullptr,
        nullptr
    );
    return result;
}

std::string Base64Encode(const unsigned char* data, std::size_t size) {
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((size + 2) / 3) * 4);
    for (std::size_t index = 0; index < size; index += 3) {
        const std::uint32_t first = data[index];
        const std::uint32_t second = index + 1 < size ? data[index + 1] : 0;
        const std::uint32_t third = index + 2 < size ? data[index + 2] : 0;
        const std::uint32_t value = (first << 16) | (second << 8) | third;
        result.push_back(alphabet[(value >> 18) & 0x3F]);
        result.push_back(alphabet[(value >> 12) & 0x3F]);
        result.push_back(
            index + 1 < size ? alphabet[(value >> 6) & 0x3F] : '='
        );
        result.push_back(
            index + 2 < size ? alphabet[value & 0x3F] : '='
        );
    }
    return result;
}

void BindSqlValue(
    sqlite3_stmt* statement,
    int index,
    const KSMSqlValue& value
) {
    const int result = std::visit(
        [statement, index](const auto& item) -> int {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                return sqlite3_bind_null(statement, index);
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                return sqlite3_bind_int64(statement, index, item);
            } else if constexpr (std::is_same_v<T, double>) {
                return sqlite3_bind_double(statement, index, item);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return sqlite3_bind_text(
                    statement,
                    index,
                    item.data(),
                    static_cast<int>(item.size()),
                    SQLITE_TRANSIENT
                );
            } else {
                return sqlite3_bind_int(statement, index, item ? 1 : 0);
            }
        },
        value
    );
    if (result != SQLITE_OK) {
        throw KSMHostException(
            "INVALID_ARGS",
            "A SQL parameter could not be bound."
        );
    }
}

KSMSqlValue ReadSqlColumn(sqlite3_stmt* statement, int column) {
    switch (sqlite3_column_type(statement, column)) {
        case SQLITE_INTEGER:
            return static_cast<std::int64_t>(
                sqlite3_column_int64(statement, column)
            );
        case SQLITE_FLOAT:
            return sqlite3_column_double(statement, column);
        case SQLITE_TEXT:
            return std::string(
                reinterpret_cast<const char*>(
                    sqlite3_column_text(statement, column)
                ),
                static_cast<std::size_t>(
                    sqlite3_column_bytes(statement, column)
                )
            );
        case SQLITE_BLOB:
            return Base64Encode(
                static_cast<const unsigned char*>(
                    sqlite3_column_blob(statement, column)
                ),
                static_cast<std::size_t>(
                    sqlite3_column_bytes(statement, column)
                )
            );
        default:
            return nullptr;
    }
}

void BindParams(
    sqlite3_stmt* statement,
    const std::vector<KSMSqlValue>& params
) {
    for (std::size_t index = 0; index < params.size(); ++index) {
        BindSqlValue(
            statement,
            static_cast<int>(index + 1),
            params[index]
        );
    }
}

}

KSMSqliteDriver::KSMSqliteDriver(
    std::shared_ptr<KSMFileSystem> file_system
) : file_system_(std::move(file_system)) {
}

std::string KSMSqliteDriver::Name() const {
    return "sqlite";
}

std::unique_ptr<KSMSqlConnection> KSMSqliteDriver::Open(
    const std::string& url
) {
    if (url != ":memory:") {
        if (
            !KSMVirtualPath::IsSafe(url) ||
            KSMVirtualPath::IsReadOnly(url) ||
            KSMVirtualPath::IsPicked(url)
        ) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "SQLite databases must use a writable Kasumi virtual path."
            );
        }
    }

    std::string database_path = url;
    if (url != ":memory:") {
        if (!file_system_) {
            throw KSMHostException(
                "NOT_SUPPORTED",
                "The filesystem is not configured."
            );
        }
        const auto resolved = file_system_->ResolveWritableFile(url);
        if (!resolved) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The database path is outside the writable sandbox."
            );
        }
        std::error_code directory_error;
        std::filesystem::create_directories(
            resolved->parent_path(),
            directory_error
        );
        if (directory_error) {
            throw KSMHostException(
                "IO_ERROR",
                "The database directory could not be created."
            );
        }
        database_path = ToUtf8(resolved->wstring());
    }

    sqlite3* database = nullptr;
    const int open_result = sqlite3_open_v2(
        database_path.c_str(),
        &database,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr
    );
    if (open_result != SQLITE_OK || database == nullptr) {
        const std::string message = database != nullptr
            ? sqlite3_errmsg(database)
            : "SQLite could not open the database.";
        if (database != nullptr) {
            sqlite3_close_v2(database);
        }
        throw KSMHostException("IO_ERROR", message);
    }
    sqlite3_busy_timeout(database, 5000);
    if (
        sqlite3_exec(
            database,
            "PRAGMA foreign_keys = ON;",
            nullptr,
            nullptr,
            nullptr
        ) != SQLITE_OK
    ) {
        const std::string message = sqlite3_errmsg(database);
        sqlite3_close_v2(database);
        throw KSMHostException("IO_ERROR", message);
    }
    return std::make_unique<KSMSqliteConnection>(database);
}

KSMSqliteConnection::KSMSqliteConnection(sqlite3* database)
    : database_(database) {
}

KSMSqliteConnection::~KSMSqliteConnection() {
    sqlite3_close_v2(database_);
}

KSMSqlQueryResult KSMSqliteConnection::Query(
    const std::string& statement,
    const std::vector<KSMSqlValue>& params
) {
    std::lock_guard lock(mutex_);
    sqlite3_stmt* prepared = nullptr;
    if (
        sqlite3_prepare_v2(
            database_,
            statement.c_str(),
            static_cast<int>(statement.size()),
            &prepared,
            nullptr
        ) != SQLITE_OK
    ) {
        throw KSMHostException("INVALID_ARGS", sqlite3_errmsg(database_));
    }

    try {
        BindParams(prepared, params);
        KSMSqlQueryResult result;
        const int column_count = sqlite3_column_count(prepared);
        for (int column = 0; column < column_count; ++column) {
            const char* name = sqlite3_column_name(prepared, column);
            result.columns.emplace_back(name != nullptr ? name : "");
        }
        while (true) {
            const int step = sqlite3_step(prepared);
            if (step == SQLITE_DONE) {
                break;
            }
            if (step != SQLITE_ROW) {
                throw KSMHostException(
                    "IO_ERROR",
                    sqlite3_errmsg(database_)
                );
            }
            std::vector<KSMSqlValue> row;
            row.reserve(static_cast<std::size_t>(column_count));
            for (int column = 0; column < column_count; ++column) {
                row.push_back(ReadSqlColumn(prepared, column));
            }
            result.rows.push_back(std::move(row));
        }
        sqlite3_finalize(prepared);
        return result;
    } catch (...) {
        sqlite3_finalize(prepared);
        throw;
    }
}

KSMSqlExecuteResult KSMSqliteConnection::Execute(
    const std::string& statement,
    const std::vector<KSMSqlValue>& params
) {
    std::lock_guard lock(mutex_);
    sqlite3_stmt* prepared = nullptr;
    if (
        sqlite3_prepare_v2(
            database_,
            statement.c_str(),
            static_cast<int>(statement.size()),
            &prepared,
            nullptr
        ) != SQLITE_OK
    ) {
        throw KSMHostException("INVALID_ARGS", sqlite3_errmsg(database_));
    }

    try {
        BindParams(prepared, params);
        const int step = sqlite3_step(prepared);
        if (step == SQLITE_ROW) {
            throw KSMHostException(
                "INVALID_ARGS",
                "The statement returns rows. Use sql.query instead."
            );
        }
        if (step != SQLITE_DONE) {
            throw KSMHostException("IO_ERROR", sqlite3_errmsg(database_));
        }
        const std::uint64_t rows_affected = static_cast<std::uint64_t>(
            sqlite3_changes64(database_)
        );
        const std::string_view kind = LeadingStatement(statement);
        const bool inserts_rows =
            StartsKeyword(kind, "insert") || StartsKeyword(kind, "replace");
        const bool mutates_rows =
            inserts_rows ||
            StartsKeyword(kind, "update") ||
            StartsKeyword(kind, "delete");
        std::optional<std::int64_t> last_insert_id;
        if (inserts_rows && rows_affected > 0) {
            last_insert_id = sqlite3_last_insert_rowid(database_);
        }
        sqlite3_finalize(prepared);
        return KSMSqlExecuteResult{
            .rows_affected = mutates_rows ? rows_affected : 0,
            .last_insert_id = last_insert_id
        };
    } catch (...) {
        sqlite3_finalize(prepared);
        throw;
    }
}
