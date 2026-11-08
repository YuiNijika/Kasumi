#include "ksm_plugin_registry.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

#include "ksm_clipboard_plugin.h"
#include "ksm_dialog_plugin.h"
#include "ksm_filesystem_plugin.h"
#include "ksm_http_plugin.h"
#include "ksm_log_plugin.h"
#include "ksm_notification_plugin.h"
#include "ksm_os_plugin.h"
#include "ksm_path_plugin.h"
#include "ksm_shell_plugin.h"
#include "ksm_sql_plugin.h"
#include "ksm_store_plugin.h"
#include "ksm_window_plugin.h"

KSMPluginRegistry::KSMPluginRegistry(
    std::vector<std::shared_ptr<KSMPlugin>> plugins
) : plugins_(std::move(plugins)) {
    for (const auto& plugin : plugins_) {
        if (!plugin) {
            throw std::invalid_argument("A Kasumi plugin cannot be null.");
        }
        const std::string& id = plugin->Id();
        if (id.empty()) {
            throw std::invalid_argument("A Kasumi plugin must have an ID.");
        }
        for (const auto& existing : plugin_ids_) {
            if (existing == id) {
                throw std::invalid_argument(
                    "Kasumi plugin IDs must be unique."
                );
            }
        }
        plugin_ids_.push_back(id);

        for (const auto& command : plugin->Commands()) {
            if (command.empty() || commands_.contains(command)) {
                throw std::invalid_argument(
                    "Kasumi plugin commands must be unique."
                );
            }
            commands_.emplace(
                command,
                Entry{plugin}
            );
            command_names_.push_back(command);
        }
        for (const auto& permission : plugin->Permissions()) {
            if (
                std::find(
                    permissions_.begin(),
                    permissions_.end(),
                    permission
                ) == permissions_.end()
            ) {
                permissions_.push_back(permission);
            }
        }
        for (const auto& event : plugin->Events()) {
            if (
                event.empty() ||
                std::find(events_.begin(), events_.end(), event) !=
                    events_.end()
            ) {
                throw std::invalid_argument(
                    "Kasumi plugin events must be unique and non-empty."
                );
            }
            events_.push_back(event);
        }
    }
}

std::optional<nlohmann::json> KSMPluginRegistry::Handle(
    const std::string& command,
    const nlohmann::json& args,
    const KSMPluginContext& context
) const {
    const auto entry = commands_.find(command);
    if (entry == commands_.end()) {
        return std::nullopt;
    }
    return entry->second.plugin->Handle(command, args, context);
}

bool KSMPluginRegistry::OwnsCommand(const std::string& command) const {
    return commands_.contains(command);
}

bool KSMPluginRegistry::KnowsPermission(
    const std::string& permission
) const {
    return std::find(
        permissions_.begin(),
        permissions_.end(),
        permission
    ) != permissions_.end();
}

bool KSMPluginRegistry::KnowsEvent(const std::string& event) const {
    return std::find(events_.begin(), events_.end(), event) != events_.end();
}

std::string KSMPluginRegistry::PluginIdForCommand(
    const std::string& command
) const {
    const auto entry = commands_.find(command);
    return entry == commands_.end() ? std::string() : entry->second.plugin->Id();
}

const std::vector<std::string>& KSMPluginRegistry::PermissionsForCommand(
    const std::string& command
) const {
    static const std::vector<std::string> empty;
    const auto entry = commands_.find(command);
    return entry == commands_.end()
        ? empty
        : entry->second.plugin->Permissions();
}

bool KSMPluginRegistry::ValidateArgs(
    const std::string& command,
    const nlohmann::json& args
) const {
    const auto entry = commands_.find(command);
    return entry != commands_.end() &&
        entry->second.plugin->ValidateArgs(command, args);
}

const std::vector<std::string>& KSMPluginRegistry::PluginIds() const {
    return plugin_ids_;
}

const std::vector<std::string>& KSMPluginRegistry::Commands() const {
    return command_names_;
}

const std::vector<std::string>& KSMPluginRegistry::Permissions() const {
    return permissions_;
}

std::vector<std::shared_ptr<KSMPlugin>> KSMDefaultPlugins() {
    return {
        std::make_shared<KSMFileSystemPlugin>(),
        std::make_shared<KSMDialogPlugin>(),
        std::make_shared<KSMClipboardPlugin>(),
        std::make_shared<KSMNotificationPlugin>(),
        std::make_shared<KSMWindowPlugin>(),
        std::make_shared<KSMHttpPlugin>(),
        std::make_shared<KSMShellPlugin>(),
        std::make_shared<KSMSqlPlugin>(),
        std::make_shared<KSMPathPlugin>(),
        std::make_shared<KSMStorePlugin>(),
        std::make_shared<KSMOperatingSystemPlugin>(),
        std::make_shared<KSMLogPlugin>()
    };
}
