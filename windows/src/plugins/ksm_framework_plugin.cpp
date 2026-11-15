#include "ksm_framework_plugin.h"

#include <stdexcept>
#include <utility>

KSMDelegatingFrameworkPlugin::KSMDelegatingFrameworkPlugin(
    std::string id,
    std::string name,
    std::string version,
    std::string description,
    std::vector<std::string> commands,
    std::vector<std::string> permissions,
    std::vector<std::string> events,
    std::vector<std::string> default_permissions,
    KSMFrameworkCommandHandler handler
)
    : id_(std::move(id)),
      name_(std::move(name)),
      version_(std::move(version)),
      description_(std::move(description)),
      commands_(std::move(commands)),
      permissions_(std::move(permissions)),
      events_(std::move(events)),
      default_permissions_(std::move(default_permissions)),
      handler_(std::move(handler)) {
    if (id_.empty()) {
        throw std::invalid_argument("A framework plugin must have an ID.");
    }
    if (!handler_) {
        throw std::invalid_argument(
            "A framework plugin must have a command handler."
        );
    }
}

const std::string& KSMDelegatingFrameworkPlugin::Id() const {
    return id_;
}

std::string KSMDelegatingFrameworkPlugin::Name() const {
    return name_;
}

std::string KSMDelegatingFrameworkPlugin::Version() const {
    return version_;
}

std::string KSMDelegatingFrameworkPlugin::Description() const {
    return description_;
}

const std::vector<std::string>&
KSMDelegatingFrameworkPlugin::Commands() const {
    return commands_;
}

const std::vector<std::string>&
KSMDelegatingFrameworkPlugin::Permissions() const {
    return permissions_;
}

const std::vector<std::string>&
KSMDelegatingFrameworkPlugin::Events() const {
    return events_;
}

const std::vector<std::string>&
KSMDelegatingFrameworkPlugin::DefaultPermissions() const {
    return default_permissions_;
}

bool KSMDelegatingFrameworkPlugin::ValidateArgs(
    const std::string&,
    const nlohmann::json& args
) const {
    return args.is_object();
}

nlohmann::json KSMDelegatingFrameworkPlugin::Handle(
    const std::string& command,
    const nlohmann::json& args,
    const KSMPluginContext& context
) {
    return handler_(command, args, context);
}
