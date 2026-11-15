#include "../plugins/ksm_framework_plugin_registry.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../generated/contract.h"
#include "../platform/ksm_host_services.h"
#include "../plugins/ksm_capability_plugin.h"
#include "../plugins/ksm_clipboard_plugin.h"
#include "../plugins/ksm_dialog_plugin.h"
#include "../plugins/ksm_filesystem_plugin.h"
#include "../plugins/ksm_http_plugin.h"
#include "../plugins/ksm_log_plugin.h"
#include "../plugins/ksm_notification_plugin.h"
#include "../plugins/ksm_os_plugin.h"
#include "../plugins/ksm_path_plugin.h"
#include "../plugins/ksm_shell_plugin.h"
#include "../plugins/ksm_sql_plugin.h"
#include "../plugins/ksm_store_plugin.h"
#include "../plugins/ksm_window_plugin.h"

namespace {

using Json = nlohmann::json;

bool Contains(
    const std::vector<std::string>& values,
    const std::string& value
) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

std::vector<std::string> StringArray(const Json& value) {
    if (!value.is_array()) {
        return {};
    }
    return value.get<std::vector<std::string>>();
}

std::vector<std::string> SelectedCommands(
    const std::vector<std::string>& enabled_commands,
    const Json& metadata
) {
    std::vector<std::string> commands;
    for (const auto& command : StringArray(
             metadata.value("commands", Json::array())
         )) {
        if (
            Contains(KSMGeneratedContract::WindowsCommands(), command) &&
            Contains(enabled_commands, command)
        ) {
            commands.push_back(command);
        }
    }
    return commands;
}

std::vector<std::string> BackendCommands(
    const std::shared_ptr<KSMPlugin>& backend,
    const std::vector<std::string>& commands
) {
    if (!backend) {
        return {};
    }
    std::vector<std::string> result;
    for (const auto& command : commands) {
        if (Contains(backend->Commands(), command)) {
            result.push_back(command);
        }
    }
    return result;
}

std::vector<std::string> CapabilityCommands(
    const std::shared_ptr<KSMPlugin>& backend,
    const std::vector<std::string>& commands
) {
    std::vector<std::string> result;
    const auto& capability_commands = KSMCapabilityCommands();
    for (const auto& command : commands) {
        if (
            Contains(capability_commands, command) &&
            (!backend || !Contains(backend->Commands(), command))
        ) {
            result.push_back(command);
        }
    }
    return result;
}

} // namespace

std::vector<std::shared_ptr<KSMFrameworkPlugin>>
KSMDefaultFrameworkPlugins(
    const std::vector<std::string>& enabled_commands,
    const std::vector<std::string>& enabled_plugins
) {
    if (enabled_commands.empty()) {
        return {};
    }

    std::unordered_map<std::string, std::shared_ptr<KSMPlugin>> backends;
    for (const auto& backend : std::vector<std::shared_ptr<KSMPlugin>>{
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
         }) {
        backends.emplace(backend->Id(), backend);
    }

    const auto& metadata = KSMGeneratedContract::PluginMetadata();
    std::vector<std::shared_ptr<KSMFrameworkPlugin>> plugins;
    for (const auto& plugin_id : KSMGeneratedContract::Plugins()) {
        if (!metadata.contains(plugin_id)) {
            continue;
        }
        if (
            !enabled_plugins.empty() &&
            !Contains(enabled_plugins, plugin_id)
        ) {
            continue;
        }
        const Json& definition = metadata.at(plugin_id);
        const auto platforms = StringArray(
            definition.value("platforms", Json::array())
        );
        if (!Contains(platforms, "windows")) {
            continue;
        }
        const auto commands = SelectedCommands(
            enabled_commands,
            definition
        );
        if (commands.empty()) {
            continue;
        }

        const auto backend_iterator = backends.find(plugin_id);
        const std::shared_ptr<KSMPlugin> backend =
            backend_iterator == backends.end()
            ? nullptr
            : backend_iterator->second;
        const auto backend_commands = BackendCommands(backend, commands);
        const auto capability_commands = CapabilityCommands(
            backend,
            commands
        );
        const std::shared_ptr<KSMPlugin> capability =
            capability_commands.empty()
            ? nullptr
            : std::make_shared<KSMCapabilityPlugin>(capability_commands);

        auto handler = [
            backend,
            backend_commands,
            capability,
            capability_commands
        ](
            const std::string& command,
            const Json& args,
            const KSMPluginContext& context
        ) {
            if (backend && Contains(backend_commands, command)) {
                return backend->Handle(command, args, context);
            }
            if (capability && Contains(capability_commands, command)) {
                return capability->Handle(command, args, context);
            }
            throw KSMHostException(
                "NOT_SUPPORTED",
                "The framework command is not implemented on this host."
            );
        };

        plugins.push_back(
            std::make_shared<KSMDelegatingFrameworkPlugin>(
                plugin_id,
                definition.value("name", plugin_id),
                definition.value("version", "1.0.0"),
                definition.value("description", ""),
                commands,
                StringArray(
                    definition.value("permissions", Json::array())
                ),
                StringArray(definition.value("events", Json::array())),
                StringArray(
                    definition.value(
                        "defaultPermissions",
                        Json::array()
                    )
                ),
                std::move(handler)
            )
        );
    }
    return plugins;
}
