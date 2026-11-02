#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ksm_host_services.h"

class KSMSqlConnection {
public:
    virtual ~KSMSqlConnection() = default;

    virtual KSMSqlQueryResult Query(
        const std::string& statement,
        const std::vector<KSMSqlValue>& params
    ) = 0;
    virtual KSMSqlExecuteResult Execute(
        const std::string& statement,
        const std::vector<KSMSqlValue>& params
    ) = 0;
};

class KSMSqlDriver {
public:
    virtual ~KSMSqlDriver() = default;

    virtual std::string Name() const = 0;
    virtual std::unique_ptr<KSMSqlConnection> Open(
        const std::string& url
    ) = 0;
};
