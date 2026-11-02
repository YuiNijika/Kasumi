#include "ksm_sql_plugin.h"

#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "../platform/ksm_host_services.h"

namespace {

using Json = nlohmann::json;

void RequirePermission(const KSMPluginContext& context) {
    if (context.has_permission && context.has_permission("sql")) {
        return;
    }
    throw KSMHostException(
        "PERMISSION_DENIED",
        "The requested permission was denied."
    );
}

KSMSqlValue SqlValueFromJson(const Json& value) {
    if (value.is_null()) {
        return nullptr;
    }
    if (value.is_boolean()) {
        return value.get<bool>();
    }
    if (value.is_number_integer()) {
        return value.get<std::int64_t>();
    }
    if (value.is_number_unsigned()) {
        const auto number = value.get<std::uint64_t>();
        if (
            number >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
        ) {
            throw KSMHostException(
                "INVALID_ARGS",
                "SQL integer parameters must fit in a signed 64-bit integer."
            );
        }
        return static_cast<std::int64_t>(number);
    }
    if (value.is_number()) {
        return value.get<double>();
    }
    if (value.is_string()) {
        return value.get<std::string>();
    }
    throw KSMHostException(
        "INVALID_ARGS",
        "SQL parameters must be strings, numbers, booleans or null."
    );
}

Json SqlValueToJson(const KSMSqlValue& value) {
    return std::visit(
        [](const auto& item) -> Json {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                return nullptr;
            } else {
                return item;
            }
        },
        value
    );
}

std::vector<KSMSqlValue> SqlParams(const Json& args) {
    std::vector<KSMSqlValue> params;
    if (!args.contains("params")) {
        return params;
    }
    for (const auto& value : args.at("params")) {
        params.push_back(SqlValueFromJson(value));
    }
    return params;
}

const std::string& RequiredString(
    const Json& args,
    const char* field
) {
    if (!args.contains(field) || !args.at(field).is_string()) {
        throw KSMHostException(
            "INVALID_ARGS",
            std::string(field) + " is required."
        );
    }
    return args.at(field).get_ref<const std::string&>();
}

Json Drivers(const KSMPluginContext& context) {
    return Json{{"drivers", context.host_services->SqlDrivers()}};
}

Json Open(
    const Json& args,
    const KSMPluginContext& context
) {
    const std::string& driver = RequiredString(args, "driver");
    const std::string& url = RequiredString(args, "url");
    const auto connection_id = context.host_services->SqlOpen(driver, url);
    if (!connection_id) {
        throw KSMHostException(
            "NOT_SUPPORTED",
            "The requested SQL driver is not available."
        );
    }
    return Json{{"connectionId", *connection_id}};
}

Json Query(
    const Json& args,
    const KSMPluginContext& context
) {
    const std::string& connection_id =
        RequiredString(args, "connectionId");
    const std::string& statement = RequiredString(args, "statement");
    const auto result = context.host_services->SqlQuery(
        connection_id,
        statement,
        SqlParams(args)
    );
    if (!result) {
        throw KSMHostException(
            "IO_ERROR",
            "The SQL query could not be completed."
        );
    }
    Json rows = Json::array();
    for (const auto& row : result->rows) {
        Json value = Json::object();
        for (
            std::size_t index = 0;
            index < row.size() && index < result->columns.size();
            ++index
        ) {
            value[result->columns[index]] = SqlValueToJson(row[index]);
        }
        rows.push_back(std::move(value));
    }
    return Json{{"columns", result->columns}, {"rows", rows}};
}

Json Execute(
    const Json& args,
    const KSMPluginContext& context
) {
    const std::string& connection_id =
        RequiredString(args, "connectionId");
    const std::string& statement = RequiredString(args, "statement");
    const auto result = context.host_services->SqlExecute(
        connection_id,
        statement,
        SqlParams(args)
    );
    if (!result) {
        throw KSMHostException(
            "IO_ERROR",
            "The SQL statement could not be executed."
        );
    }
    Json last_insert_id = nullptr;
    if (result->last_insert_id) {
        last_insert_id = *result->last_insert_id;
    }
    return Json{
        {"rowsAffected", result->rows_affected},
        {"lastInsertId", last_insert_id}
    };
}

Json Close(
    const Json& args,
    const KSMPluginContext& context
) {
    const std::string& connection_id =
        RequiredString(args, "connectionId");
    return Json{{"closed", context.host_services->SqlClose(connection_id)}};
}

}

const std::string& KSMSqlPlugin::Id() const {
    static const std::string id = "sql";
    return id;
}

const std::vector<std::string>& KSMSqlPlugin::Commands() const {
    static const std::vector<std::string> commands = {
        "sql.drivers",
        "sql.open",
        "sql.query",
        "sql.execute",
        "sql.close"
    };
    return commands;
}

const std::vector<std::string>& KSMSqlPlugin::Permissions() const {
    static const std::vector<std::string> permissions = {"sql"};
    return permissions;
}

Json KSMSqlPlugin::Handle(
    const std::string& command,
    const Json& args,
    const KSMPluginContext& context
) {
    if (!context.host_services) {
        throw KSMHostException(
            "NOT_SUPPORTED",
            "The SQL service is not configured."
        );
    }
    RequirePermission(context);

    if (command == "sql.drivers") {
        return Drivers(context);
    }
    if (command == "sql.open") {
        return Open(args, context);
    }
    if (command == "sql.query") {
        return Query(args, context);
    }
    if (command == "sql.execute") {
        return Execute(args, context);
    }
    return Close(args, context);
}
