#include "ksm_odbc_sql_driver.h"

#include <windows.h>
#include <sqlext.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {

constexpr std::size_t kMaxUrlLength = 4096;
constexpr std::size_t kMaxResultTextBytes = 1024 * 1024;
constexpr std::size_t kResultTextCharacters = kMaxResultTextBytes / sizeof(wchar_t);
constexpr int kDefaultMysqlPort = 3306;
constexpr int kDefaultPostgresPort = 5432;

struct RemoteUrl {
    std::string host;
    int port;
    std::string database;
    std::string username;
    std::string credential;
    bool require_tls;
};

struct OdbcDiagnostic {
    std::string state;
    std::string message;
};

std::wstring ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0
    );
    if (size <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        size
    );
    return result;
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

std::string LowerAscii(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        }
    );
    return value;
}

bool StartsWithScheme(const std::string& url, const std::string& scheme) {
    if (url.size() < scheme.size() + 3) {
        return false;
    }
    if (LowerAscii(url.substr(0, scheme.size())) != scheme) {
        return false;
    }
    return url.compare(scheme.size(), 3, "://") == 0;
}

int HexValue(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

std::optional<std::string> PercentDecode(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '%') {
            result.push_back(value[index]);
            continue;
        }
        if (index + 2 >= value.size()) {
            return std::nullopt;
        }
        const int high = HexValue(value[index + 1]);
        const int low = HexValue(value[index + 2]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        const char decoded = static_cast<char>((high << 4) | low);
        if (decoded == '\0') {
            return std::nullopt;
        }
        result.push_back(decoded);
        index += 2;
    }
    return result;
}

bool HasForbiddenHostCharacter(const std::string& host) {
    return std::any_of(
        host.begin(),
        host.end(),
        [](unsigned char character) {
            return std::isspace(character) != 0 ||
                character == '/' ||
                character == '\\' ||
                character == '?' ||
                character == '#';
        }
    );
}

std::optional<int> ParsePort(const std::string& value) {
    if (value.empty()) {
        return std::nullopt;
    }
    int port = 0;
    for (char character : value) {
        if (character < '0' || character > '9') {
            return std::nullopt;
        }
        port = port * 10 + (character - '0');
        if (port > 65535) {
            return std::nullopt;
        }
    }
    if (port == 0) {
        return std::nullopt;
    }
    return port;
}

bool ParseTlsQuery(
    const std::string& query,
    std::string_view tls_key,
    bool& require_tls
) {
    if (query.empty()) {
        return true;
    }
    std::size_t position = 0;
    while (position <= query.size()) {
        const std::size_t end = query.find('&', position);
        const std::string item = query.substr(
            position,
            end == std::string::npos ? std::string::npos : end - position
        );
        if (item.empty()) {
            return false;
        }
        const std::size_t equal = item.find('=');
        if (equal == std::string::npos) {
            return false;
        }
        const std::string key = LowerAscii(item.substr(0, equal));
        const std::string value = LowerAscii(item.substr(equal + 1));
        if (key != tls_key) {
            return false;
        }
        if (
            value == "require" ||
            value == "required" ||
            value == "verify-ca" ||
            value == "verify-full"
        ) {
            require_tls = true;
        } else if (value == "disable" || value == "disabled") {
            require_tls = false;
        } else {
            return false;
        }
        if (end == std::string::npos) {
            break;
        }
        position = end + 1;
    }
    return true;
}

std::optional<RemoteUrl> ParseRemoteUrl(
    const std::string& url,
    const std::string& scheme,
    int default_port,
    std::string_view tls_key
) {
    if (url.empty() || url.size() > kMaxUrlLength || !StartsWithScheme(url, scheme)) {
        return std::nullopt;
    }
    if (std::any_of(
        url.begin(),
        url.end(),
        [](unsigned char character) {
            return character < 0x20 || character == 0x7F;
        }
    )) {
        return std::nullopt;
    }
    if (url.find('#') != std::string::npos) {
        return std::nullopt;
    }

    const std::size_t authority_start = scheme.size() + 3;
    const std::size_t authority_end = url.find_first_of("/?", authority_start);
    const std::string authority = url.substr(
        authority_start,
        authority_end == std::string::npos
            ? std::string::npos
            : authority_end - authority_start
    );
    if (authority.empty()) {
        return std::nullopt;
    }

    std::string host_and_port = authority;
    std::string username;
    std::string credential;
    const std::size_t at = authority.rfind('@');
    if (at != std::string::npos) {
        const std::string user_info = authority.substr(0, at);
        host_and_port = authority.substr(at + 1);
        const std::size_t colon = user_info.find(':');
        const auto decoded_user = PercentDecode(
            colon == std::string::npos ? user_info : user_info.substr(0, colon)
        );
        const auto decoded_credential = PercentDecode(
            colon == std::string::npos ? "" : user_info.substr(colon + 1)
        );
        if (!decoded_user || !decoded_credential) {
            return std::nullopt;
        }
        username = *decoded_user;
        credential = *decoded_credential;
    }

    std::string host;
    std::string port_value;
    if (!host_and_port.empty() && host_and_port.front() == '[') {
        const std::size_t closing = host_and_port.find(']');
        if (closing == std::string::npos) {
            return std::nullopt;
        }
        host = host_and_port.substr(1, closing - 1);
        if (closing + 1 < host_and_port.size()) {
            if (host_and_port[closing + 1] != ':') {
                return std::nullopt;
            }
            port_value = host_and_port.substr(closing + 2);
        }
    } else {
        const std::size_t colon = host_and_port.rfind(':');
        if (colon != std::string::npos) {
            host = host_and_port.substr(0, colon);
            port_value = host_and_port.substr(colon + 1);
        } else {
            host = host_and_port;
        }
    }
    if (host.empty() || HasForbiddenHostCharacter(host)) {
        return std::nullopt;
    }

    int port = default_port;
    if (!port_value.empty()) {
        const auto parsed_port = ParsePort(port_value);
        if (!parsed_port) {
            return std::nullopt;
        }
        port = *parsed_port;
    }

    std::string database;
    std::string query;
    if (authority_end != std::string::npos) {
        const std::size_t query_at = url.find('?', authority_end);
        if (authority_end < url.size() && url[authority_end] == '/') {
            const auto decoded_database = PercentDecode(url.substr(
                authority_end + 1,
                query_at == std::string::npos
                    ? std::string::npos
                    : query_at - authority_end - 1
            ));
            if (!decoded_database || decoded_database->find('/') != std::string::npos) {
                return std::nullopt;
            }
            database = *decoded_database;
        }
        if (query_at != std::string::npos) {
            query = url.substr(query_at + 1);
        }
    }

    bool require_tls = true;
    if (!ParseTlsQuery(query, tls_key, require_tls)) {
        return std::nullopt;
    }
    return RemoteUrl{
        .host = std::move(host),
        .port = port,
        .database = std::move(database),
        .username = std::move(username),
        .credential = std::move(credential),
        .require_tls = require_tls
    };
}

std::wstring QuoteOdbcValue(const std::string& value) {
    std::wstring wide = ToWide(value);
    std::wstring quoted;
    quoted.reserve(wide.size() + 4);
    quoted.push_back(L'{');
    for (wchar_t character : wide) {
        if (character == L'}') {
            quoted.push_back(L'}');
        }
        quoted.push_back(character);
    }
    quoted.push_back(L'}');
    return quoted;
}

std::wstring BuildConnectionString(
    const std::string& odbc_driver,
    const std::string& scheme,
    const RemoteUrl& url
) {
    std::wstring result = L"Driver=" + QuoteOdbcValue(odbc_driver) +
        L";Server=" + QuoteOdbcValue(url.host) +
        L";Port=" + std::to_wstring(url.port) +
        L";Database=" + QuoteOdbcValue(url.database);
    if (!url.username.empty()) {
        result += scheme == "mysql"
            ? L";User=" + QuoteOdbcValue(url.username)
            : L";Uid=" + QuoteOdbcValue(url.username);
    }
    if (!url.credential.empty()) {
        result += scheme == "mysql"
            ? L";Password=" + QuoteOdbcValue(url.credential)
            : L";Pwd=" + QuoteOdbcValue(url.credential);
    }
    if (scheme == "mysql") {
        result += url.require_tls ? L";sslMode=REQUIRED" : L";sslMode=DISABLED";
    } else {
        result += url.require_tls ? L";SSLmode=require" : L";SSLmode=disable";
    }
    result.push_back(L';');
    return result;
}

OdbcDiagnostic ReadDiagnostic(SQLSMALLINT handle_type, SQLHANDLE handle) {
    SQLWCHAR state[6] = {};
    SQLWCHAR message[1024] = {};
    SQLINTEGER native_error = 0;
    SQLSMALLINT message_length = 0;
    const SQLRETURN result = SQLGetDiagRecW(
        handle_type,
        handle,
        1,
        state,
        &native_error,
        message,
        static_cast<SQLSMALLINT>(std::size(message)),
        &message_length
    );
    if (!SQL_SUCCEEDED(result)) {
        return OdbcDiagnostic{};
    }
    return OdbcDiagnostic{
        .state = ToUtf8(std::wstring(state)),
        .message = ToUtf8(std::wstring(message, message_length))
    };
}

[[noreturn]] void ThrowDiagnostic(
    SQLSMALLINT handle_type,
    SQLHANDLE handle,
    const std::string& fallback
) {
    const OdbcDiagnostic diagnostic = ReadDiagnostic(handle_type, handle);
    const std::string state = diagnostic.state.empty() ? "" : diagnostic.state + " ";
    const std::string message = diagnostic.message.empty()
        ? fallback
        : state + diagnostic.message;
    const std::string code = diagnostic.state.starts_with("42")
        ? "INVALID_ARGS"
        : "IO_ERROR";
    throw KSMHostException(code, message);
}

std::vector<std::string> EnumerateOdbcDrivers() {
    SQLHENV environment = SQL_NULL_HENV;
    if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &environment) != SQL_SUCCESS) {
        return {};
    }
    if (!SQL_SUCCEEDED(SQLSetEnvAttr(
        environment,
        SQL_ATTR_ODBC_VERSION,
        reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3_80),
        0
    ))) {
        SQLFreeHandle(SQL_HANDLE_ENV, environment);
        return {};
    }

    std::vector<std::string> drivers;
    SQLWCHAR description[512] = {};
    SQLWCHAR attributes[2048] = {};
    SQLSMALLINT description_length = 0;
    SQLSMALLINT attributes_length = 0;
    SQLUSMALLINT direction = SQL_FETCH_FIRST;
    while (true) {
        const SQLRETURN result = SQLDriversW(
            environment,
            direction,
            description,
            static_cast<SQLSMALLINT>(std::size(description)),
            &description_length,
            attributes,
            static_cast<SQLSMALLINT>(std::size(attributes)),
            &attributes_length
        );
        if (result == SQL_NO_DATA) {
            break;
        }
        if (!SQL_SUCCEEDED(result)) {
            drivers.clear();
            break;
        }
        if (description_length > 0) {
            drivers.push_back(ToUtf8(std::wstring(
                description,
                static_cast<std::size_t>(description_length)
            )));
        }
        direction = SQL_FETCH_NEXT;
    }
    SQLFreeHandle(SQL_HANDLE_ENV, environment);
    return drivers;
}

std::string FindDriver(
    const std::vector<std::string>& drivers,
    const std::vector<std::string>& keywords
) {
    for (const auto& driver : drivers) {
        const std::string lower = LowerAscii(driver);
        for (const auto& keyword : keywords) {
            if (lower.find(keyword) != std::string::npos) {
                return driver;
            }
        }
    }
    return {};
}

std::string StringFromValue(const KSMSqlValue& value) {
    return std::visit(
        [](const auto& item) -> std::string {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                return {};
            } else if constexpr (std::is_same_v<T, bool>) {
                return item ? "1" : "0";
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                return std::to_string(item);
            } else if constexpr (std::is_same_v<T, double>) {
                char buffer[64] = {};
                std::snprintf(buffer, sizeof(buffer), "%.17g", item);
                return buffer;
            } else {
                return item;
            }
        },
        value
    );
}

struct BoundParam {
    std::string value;
    SQLLEN indicator = 0;
};

void BindParams(
    SQLHSTMT statement,
    const std::vector<KSMSqlValue>& params,
    std::vector<BoundParam>& buffers
) {
    buffers.resize(params.size());
    for (std::size_t index = 0; index < params.size(); ++index) {
        const bool is_null = std::holds_alternative<std::nullptr_t>(params[index]);
        buffers[index].value = StringFromValue(params[index]);
        buffers[index].indicator = is_null
            ? SQL_NULL_DATA
            : static_cast<SQLLEN>(buffers[index].value.size());
        const SQLRETURN result = SQLBindParameter(
            statement,
            static_cast<SQLUSMALLINT>(index + 1),
            SQL_PARAM_INPUT,
            SQL_C_CHAR,
            SQL_VARCHAR,
            static_cast<SQLULEN>(std::max<std::size_t>(
                1,
                buffers[index].value.size()
            )),
            0,
            buffers[index].value.data(),
            static_cast<SQLLEN>(buffers[index].value.size()),
            &buffers[index].indicator
        );
        if (!SQL_SUCCEEDED(result)) {
            ThrowDiagnostic(SQL_HANDLE_STMT, statement, "SQL parameters could not be bound.");
        }
    }
}

std::string Base64Encode(const std::vector<unsigned char>& data) {
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);
    for (std::size_t index = 0; index < data.size(); index += 3) {
        const std::uint32_t first = data[index];
        const std::uint32_t second = index + 1 < data.size() ? data[index + 1] : 0;
        const std::uint32_t third = index + 2 < data.size() ? data[index + 2] : 0;
        const std::uint32_t value = (first << 16) | (second << 8) | third;
        result.push_back(alphabet[(value >> 18) & 0x3F]);
        result.push_back(alphabet[(value >> 12) & 0x3F]);
        result.push_back(
            index + 1 < data.size() ? alphabet[(value >> 6) & 0x3F] : '='
        );
        result.push_back(
            index + 2 < data.size() ? alphabet[value & 0x3F] : '='
        );
    }
    return result;
}

KSMSqlValue ReadColumn(SQLHSTMT statement, SQLUSMALLINT column, SQLSMALLINT type) {
    SQLLEN indicator = 0;
    if (
        type == SQL_INTEGER ||
        type == SQL_SMALLINT ||
        type == SQL_TINYINT ||
        type == SQL_BIGINT
    ) {
        SQLBIGINT value = 0;
        const SQLRETURN result = SQLGetData(
            statement,
            column,
            SQL_C_SBIGINT,
            &value,
            0,
            &indicator
        );
        if (indicator == SQL_NULL_DATA) {
            return nullptr;
        }
        if (!SQL_SUCCEEDED(result)) {
            ThrowDiagnostic(SQL_HANDLE_STMT, statement, "SQL integer column could not be read.");
        }
        return static_cast<std::int64_t>(value);
    }
    if (
        type == SQL_FLOAT ||
        type == SQL_DOUBLE ||
        type == SQL_REAL ||
        type == SQL_DECIMAL ||
        type == SQL_NUMERIC
    ) {
        double value = 0;
        const SQLRETURN result = SQLGetData(
            statement,
            column,
            SQL_C_DOUBLE,
            &value,
            0,
            &indicator
        );
        if (indicator == SQL_NULL_DATA) {
            return nullptr;
        }
        if (!SQL_SUCCEEDED(result) || !std::isfinite(value)) {
            ThrowDiagnostic(SQL_HANDLE_STMT, statement, "SQL numeric column could not be read.");
        }
        return value;
    }
    if (type == SQL_BIT) {
        unsigned char value = 0;
        const SQLRETURN result = SQLGetData(
            statement,
            column,
            SQL_C_BIT,
            &value,
            0,
            &indicator
        );
        if (indicator == SQL_NULL_DATA) {
            return nullptr;
        }
        if (!SQL_SUCCEEDED(result)) {
            ThrowDiagnostic(SQL_HANDLE_STMT, statement, "SQL boolean column could not be read.");
        }
        return value != 0;
    }
    if (type == SQL_BINARY || type == SQL_VARBINARY || type == SQL_LONGVARBINARY) {
        std::vector<unsigned char> value(kMaxResultTextBytes);
        const SQLRETURN result = SQLGetData(
            statement,
            column,
            SQL_C_BINARY,
            value.data(),
            static_cast<SQLLEN>(value.size()),
            &indicator
        );
        if (indicator == SQL_NULL_DATA) {
            return nullptr;
        }
        if (!SQL_SUCCEEDED(result) || indicator < 0 ||
            static_cast<std::size_t>(indicator) > value.size()) {
            throw KSMHostException(
                "INVALID_ARGS",
                "SQL binary result exceeds 1 MiB."
            );
        }
        value.resize(static_cast<std::size_t>(indicator));
        return Base64Encode(value);
    }

    std::vector<wchar_t> value(kResultTextCharacters);
    const SQLRETURN result = SQLGetData(
        statement,
        column,
        SQL_C_WCHAR,
        value.data(),
        static_cast<SQLLEN>(value.size() * sizeof(wchar_t)),
        &indicator
    );
    if (indicator == SQL_NULL_DATA) {
        return nullptr;
    }
    if (!SQL_SUCCEEDED(result) || indicator < 0 ||
        static_cast<std::size_t>(indicator) > value.size() * sizeof(wchar_t)) {
        throw KSMHostException(
            "INVALID_ARGS",
            "SQL text result exceeds 1 MiB."
        );
    }
    const std::size_t characters = static_cast<std::size_t>(indicator) / sizeof(wchar_t);
    return ToUtf8(std::wstring(value.data(), characters));
}

void PrepareStatement(
    SQLHSTMT statement,
    const std::string& sql,
    const std::vector<KSMSqlValue>& params,
    std::vector<BoundParam>& buffers
) {
    const std::wstring wide = ToWide(sql);
    if (wide.empty()) {
        throw KSMHostException("INVALID_ARGS", "SQL statement is invalid.");
    }
    if (!SQL_SUCCEEDED(SQLPrepareW(
        statement,
        const_cast<SQLWCHAR*>(wide.data()),
        static_cast<SQLINTEGER>(wide.size())
    ))) {
        ThrowDiagnostic(SQL_HANDLE_STMT, statement, "SQL statement could not be prepared.");
    }
    BindParams(statement, params, buffers);
    if (!SQL_SUCCEEDED(SQLExecute(statement))) {
        ThrowDiagnostic(SQL_HANDLE_STMT, statement, "SQL statement could not be executed.");
    }
}

}

KSMOdbcSqlDriver::KSMOdbcSqlDriver(
    std::string name,
    std::string scheme,
    std::string odbc_driver,
    int default_port
) : name_(std::move(name)),
    scheme_(std::move(scheme)),
    odbc_driver_(std::move(odbc_driver)),
    default_port_(default_port) {
}

std::vector<std::shared_ptr<KSMSqlDriver>> KSMOdbcSqlDriver::Discover() {
    const std::vector<std::string> installed = EnumerateOdbcDrivers();
    if (installed.empty()) {
        return {};
    }

    std::vector<std::shared_ptr<KSMSqlDriver>> drivers;
    const std::string mysql = FindDriver(
        installed,
        {"mysql", "mariadb"}
    );
    if (!mysql.empty()) {
        drivers.push_back(std::make_shared<KSMOdbcSqlDriver>(
            "mysql",
            "mysql",
            mysql,
            kDefaultMysqlPort
        ));
    }
    const std::string postgres = FindDriver(
        installed,
        {"postgresql", "psqlodbc", "postgres"}
    );
    if (!postgres.empty()) {
        drivers.push_back(std::make_shared<KSMOdbcSqlDriver>(
            "postgres",
            "postgresql",
            postgres,
            kDefaultPostgresPort
        ));
    }
    return drivers;
}

std::string KSMOdbcSqlDriver::Name() const {
    return name_;
}

std::unique_ptr<KSMSqlConnection> KSMOdbcSqlDriver::Open(
    const std::string& url
) {
    const bool is_mysql = scheme_ == "mysql";
    const auto parsed = ParseRemoteUrl(
        url,
        scheme_,
        default_port_,
        is_mysql ? std::string_view("ssl-mode") : std::string_view("sslmode")
    );
    if (!parsed) {
        throw KSMHostException(
            "INVALID_ARGS",
            is_mysql
                ? "MySQL URLs must use mysql://host[:port]/database."
                : "PostgreSQL URLs must use postgresql://host[:port]/database."
        );
    }

    SQLHENV environment = SQL_NULL_HENV;
    SQLHDBC connection = SQL_NULL_HDBC;
    if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &environment) != SQL_SUCCESS) {
        throw KSMHostException("IO_ERROR", "The ODBC environment could not be created.");
    }
    if (!SQL_SUCCEEDED(SQLSetEnvAttr(
        environment,
        SQL_ATTR_ODBC_VERSION,
        reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3_80),
        0
    ))) {
        SQLFreeHandle(SQL_HANDLE_ENV, environment);
        throw KSMHostException(
            "IO_ERROR",
            "The ODBC environment could not be configured."
        );
    }
    if (SQLAllocHandle(SQL_HANDLE_DBC, environment, &connection) != SQL_SUCCESS) {
        SQLFreeHandle(SQL_HANDLE_ENV, environment);
        throw KSMHostException("IO_ERROR", "The ODBC connection could not be created.");
    }

    SQLSetConnectAttr(
        connection,
        SQL_ATTR_LOGIN_TIMEOUT,
        reinterpret_cast<SQLPOINTER>(15),
        0
    );
    const std::wstring connection_string = BuildConnectionString(
        odbc_driver_,
        scheme_,
        *parsed
    );
    const SQLRETURN result = SQLDriverConnectW(
        connection,
        nullptr,
        const_cast<SQLWCHAR*>(connection_string.c_str()),
        SQL_NTS,
        nullptr,
        0,
        nullptr,
        SQL_DRIVER_NOPROMPT
    );
    if (!SQL_SUCCEEDED(result)) {
        const OdbcDiagnostic diagnostic = ReadDiagnostic(SQL_HANDLE_DBC, connection);
        SQLDisconnect(connection);
        SQLFreeHandle(SQL_HANDLE_DBC, connection);
        SQLFreeHandle(SQL_HANDLE_ENV, environment);
        throw KSMHostException(
            "IO_ERROR",
            diagnostic.message.empty()
                ? "The remote database connection failed."
                : diagnostic.message
        );
    }
    return std::make_unique<KSMOdbcSqlConnection>(environment, connection);
}

KSMOdbcSqlConnection::KSMOdbcSqlConnection(
    void* environment,
    void* connection
) : environment_(environment), connection_(connection) {
}

KSMOdbcSqlConnection::~KSMOdbcSqlConnection() {
    if (connection_ != nullptr) {
        SQLDisconnect(static_cast<SQLHDBC>(connection_));
        SQLFreeHandle(SQL_HANDLE_DBC, static_cast<SQLHDBC>(connection_));
    }
    if (environment_ != nullptr) {
        SQLFreeHandle(SQL_HANDLE_ENV, static_cast<SQLHENV>(environment_));
    }
}

KSMSqlQueryResult KSMOdbcSqlConnection::Query(
    const std::string& statement,
    const std::vector<KSMSqlValue>& params
) {
    std::lock_guard lock(mutex_);
    SQLHSTMT handle = SQL_NULL_HSTMT;
    if (
        SQLAllocHandle(
            SQL_HANDLE_STMT,
            static_cast<SQLHDBC>(connection_),
            &handle
        ) != SQL_SUCCESS
    ) {
        throw KSMHostException("IO_ERROR", "The SQL query could not be created.");
    }
    try {
        std::vector<BoundParam> buffers;
        buffers.reserve(params.size());
        PrepareStatement(handle, statement, params, buffers);

        SQLSMALLINT column_count = 0;
        if (
            !SQL_SUCCEEDED(SQLNumResultCols(handle, &column_count)) ||
            column_count <= 0
        ) {
            throw KSMHostException(
                "INVALID_ARGS",
                "The statement does not return rows. Use sql.execute."
            );
        }
        KSMSqlQueryResult result;
        result.columns.reserve(static_cast<std::size_t>(column_count));
        std::vector<SQLSMALLINT> types;
        types.reserve(static_cast<std::size_t>(column_count));
        for (SQLSMALLINT column = 1; column <= column_count; ++column) {
            SQLWCHAR name[256] = {};
            SQLSMALLINT name_length = 0;
            SQLSMALLINT data_type = 0;
            SQLULEN column_size = 0;
            SQLSMALLINT decimal_digits = 0;
            SQLSMALLINT nullable = 0;
            if (
                !SQL_SUCCEEDED(SQLDescribeColW(
                    handle,
                    static_cast<SQLUSMALLINT>(column),
                    name,
                    static_cast<SQLSMALLINT>(std::size(name)),
                    &name_length,
                    &data_type,
                    &column_size,
                    &decimal_digits,
                    &nullable
                ))
            ) {
                ThrowDiagnostic(SQL_HANDLE_STMT, handle, "SQL column metadata could not be read.");
            }
            result.columns.push_back(ToUtf8(std::wstring(
                name,
                static_cast<std::size_t>(name_length)
            )));
            types.push_back(data_type);
        }

        while (true) {
            const SQLRETURN fetch = SQLFetch(handle);
            if (fetch == SQL_NO_DATA) {
                break;
            }
            if (!SQL_SUCCEEDED(fetch)) {
                ThrowDiagnostic(SQL_HANDLE_STMT, handle, "SQL rows could not be read.");
            }
            std::vector<KSMSqlValue> row;
            row.reserve(static_cast<std::size_t>(column_count));
            for (SQLSMALLINT column = 1; column <= column_count; ++column) {
                row.push_back(ReadColumn(
                    handle,
                    static_cast<SQLUSMALLINT>(column),
                    types[static_cast<std::size_t>(column - 1)]
                ));
            }
            result.rows.push_back(std::move(row));
        }
        SQLFreeHandle(SQL_HANDLE_STMT, handle);
        return result;
    } catch (...) {
        SQLFreeHandle(SQL_HANDLE_STMT, handle);
        throw;
    }
}

KSMSqlExecuteResult KSMOdbcSqlConnection::Execute(
    const std::string& statement,
    const std::vector<KSMSqlValue>& params
) {
    std::lock_guard lock(mutex_);
    SQLHSTMT handle = SQL_NULL_HSTMT;
    if (
        SQLAllocHandle(
            SQL_HANDLE_STMT,
            static_cast<SQLHDBC>(connection_),
            &handle
        ) != SQL_SUCCESS
    ) {
        throw KSMHostException("IO_ERROR", "The SQL statement could not be created.");
    }
    try {
        std::vector<BoundParam> buffers;
        buffers.reserve(params.size());
        PrepareStatement(handle, statement, params, buffers);

        SQLSMALLINT column_count = 0;
        if (!SQL_SUCCEEDED(SQLNumResultCols(handle, &column_count))) {
            ThrowDiagnostic(SQL_HANDLE_STMT, handle, "SQL result metadata could not be read.");
        }
        if (column_count > 0) {
            throw KSMHostException(
                "INVALID_ARGS",
                "The statement returns rows. Use sql.query."
            );
        }

        SQLLEN affected = 0;
        if (!SQL_SUCCEEDED(SQLRowCount(handle, &affected))) {
            ThrowDiagnostic(SQL_HANDLE_STMT, handle, "SQL affected row count could not be read.");
        }
        SQLFreeHandle(SQL_HANDLE_STMT, handle);
        return KSMSqlExecuteResult{
            .rows_affected = affected > 0
                ? static_cast<std::uint64_t>(affected)
                : 0,
            .last_insert_id = std::nullopt
        };
    } catch (...) {
        SQLFreeHandle(SQL_HANDLE_STMT, handle);
        throw;
    }
}
