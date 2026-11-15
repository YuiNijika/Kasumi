#include "ksm_plugin_registry.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

KSMAppPluginRegistry::KSMAppPluginRegistry(
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
        for (const auto& event : plugin->FrameworkEvents()) {
            if (
                event.empty() ||
                std::find(
                    framework_events_.begin(),
                    framework_events_.end(),
                    event
                ) == framework_events_.end()
            ) {
                framework_events_.push_back(event);
            }
        }
    }
}

KSMAppPluginRegistry::~KSMAppPluginRegistry() {
    Stop();
}

void KSMAppPluginRegistry::Start(const KSMPluginContext& context) {
    if (!started_.empty()) {
        return;
    }
    try {
        for (const auto& plugin : plugins_) {
            plugin->Start(context);
            started_.push_back(plugin);
        }
    } catch (...) {
        Stop();
        throw;
    }
}

void KSMAppPluginRegistry::Stop() noexcept {
    for (auto iterator = started_.rbegin(); iterator != started_.rend(); ++iterator) {
        try {
            (*iterator)->Stop();
        } catch (...) {
        }
    }
    started_.clear();
}

void KSMAppPluginRegistry::OnFrameworkEvent(
    const std::string& event,
    const nlohmann::json& payload
) const {
    for (const auto& plugin : plugins_) {
        const auto& watched = plugin->FrameworkEvents();
        if (
            std::find(watched.begin(), watched.end(), event) != watched.end()
        ) {
            plugin->OnFrameworkEvent(event, payload);
        }
    }
}

std::optional<nlohmann::json> KSMAppPluginRegistry::Handle(
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

bool KSMAppPluginRegistry::OwnsCommand(const std::string& command) const {
    return commands_.contains(command);
}

bool KSMAppPluginRegistry::KnowsPlugin(
    const std::string& plugin_id
) const {
    return std::find(
        plugin_ids_.begin(),
        plugin_ids_.end(),
        plugin_id
    ) != plugin_ids_.end();
}

bool KSMAppPluginRegistry::KnowsPermission(
    const std::string& permission
) const {
    return std::find(
        permissions_.begin(),
        permissions_.end(),
        permission
    ) != permissions_.end();
}

bool KSMAppPluginRegistry::KnowsEvent(const std::string& event) const {
    return std::find(events_.begin(), events_.end(), event) != events_.end();
}

std::string KSMAppPluginRegistry::PluginIdForCommand(
    const std::string& command
) const {
    const auto entry = commands_.find(command);
    return entry == commands_.end() ? std::string() : entry->second.plugin->Id();
}

const std::vector<std::string>& KSMAppPluginRegistry::PermissionsForCommand(
    const std::string& command
) const {
    static const std::vector<std::string> empty;
    const auto entry = commands_.find(command);
    return entry == commands_.end()
        ? empty
        : entry->second.plugin->Permissions();
}

bool KSMAppPluginRegistry::ValidateArgs(
    const std::string& command,
    const nlohmann::json& args
) const {
    const auto entry = commands_.find(command);
    return entry != commands_.end() &&
        entry->second.plugin->ValidateArgs(command, args);
}

const std::vector<std::string>& KSMAppPluginRegistry::PluginIds() const {
    return plugin_ids_;
}

const std::vector<std::string>& KSMAppPluginRegistry::Commands() const {
    return command_names_;
}

const std::vector<std::string>& KSMAppPluginRegistry::Permissions() const {
    return permissions_;
}

const std::vector<std::string>& KSMAppPluginRegistry::Events() const {
    return events_;
}

const std::vector<std::string>&
KSMAppPluginRegistry::FrameworkEvents() const {
    return framework_events_;
}

std::string KSMAppPluginRegistry::NameForPlugin(
    const std::string& plugin_id
) const {
    for (const auto& plugin : plugins_) {
        if (plugin->Id() == plugin_id) {
            return plugin->Name();
        }
    }
    return plugin_id;
}

std::string KSMAppPluginRegistry::VersionForPlugin(
    const std::string& plugin_id
) const {
    for (const auto& plugin : plugins_) {
        if (plugin->Id() == plugin_id) {
            return plugin->Version();
        }
    }
    return "1.0.0";
}

std::string KSMAppPluginRegistry::DescriptionForPlugin(
    const std::string& plugin_id
) const {
    for (const auto& plugin : plugins_) {
        if (plugin->Id() == plugin_id) {
            return plugin->Description();
        }
    }
    return {};
}
