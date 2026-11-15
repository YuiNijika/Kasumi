#include "ksm_framework_plugin_registry.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

KSMFrameworkPluginRegistry::KSMFrameworkPluginRegistry(
    std::vector<std::shared_ptr<KSMFrameworkPlugin>> plugins
) : plugins_(std::move(plugins)) {
    for (const auto& plugin : plugins_) {
        if (!plugin) {
            throw std::invalid_argument(
                "A Kasumi framework plugin cannot be null."
            );
        }
        const std::string& id = plugin->Id();
        if (id.empty()) {
            throw std::invalid_argument(
                "A Kasumi framework plugin must have an ID."
            );
        }
        if (
            std::find(plugin_ids_.begin(), plugin_ids_.end(), id) !=
            plugin_ids_.end()
        ) {
            throw std::invalid_argument(
                "Kasumi framework plugin IDs must be unique."
            );
        }
        plugin_ids_.push_back(id);

        for (const auto& command : plugin->Commands()) {
            if (command.empty() || commands_.contains(command)) {
                throw std::invalid_argument(
                    "Kasumi framework plugin commands must be unique."
                );
            }
            commands_.emplace(command, Entry{plugin});
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
                    "Kasumi framework plugin events must be unique."
                );
            }
            events_.push_back(event);
        }
    }
}

KSMFrameworkPluginRegistry::~KSMFrameworkPluginRegistry() {
    Stop();
}

void KSMFrameworkPluginRegistry::Start(const KSMPluginContext& context) {
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

void KSMFrameworkPluginRegistry::Stop() noexcept {
    for (
        auto iterator = started_.rbegin();
        iterator != started_.rend();
        ++iterator
    ) {
        try {
            (*iterator)->Stop();
        } catch (...) {
        }
    }
    started_.clear();
}

std::optional<nlohmann::json> KSMFrameworkPluginRegistry::Handle(
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

bool KSMFrameworkPluginRegistry::OwnsCommand(
    const std::string& command
) const {
    return commands_.contains(command);
}

bool KSMFrameworkPluginRegistry::KnowsPermission(
    const std::string& permission
) const {
    return std::find(
        permissions_.begin(),
        permissions_.end(),
        permission
    ) != permissions_.end();
}

bool KSMFrameworkPluginRegistry::KnowsEvent(
    const std::string& event
) const {
    return std::find(events_.begin(), events_.end(), event) != events_.end();
}

std::string KSMFrameworkPluginRegistry::PluginIdForCommand(
    const std::string& command
) const {
    const auto entry = commands_.find(command);
    return entry == commands_.end() ? std::string() : entry->second.plugin->Id();
}

const std::vector<std::string>&
KSMFrameworkPluginRegistry::PermissionsForCommand(
    const std::string& command
) const {
    static const std::vector<std::string> empty;
    const auto entry = commands_.find(command);
    return entry == commands_.end()
        ? empty
        : entry->second.plugin->Permissions();
}

bool KSMFrameworkPluginRegistry::ValidateArgs(
    const std::string& command,
    const nlohmann::json& args
) const {
    const auto entry = commands_.find(command);
    return entry != commands_.end() &&
        entry->second.plugin->ValidateArgs(command, args);
}

const std::vector<std::string>&
KSMFrameworkPluginRegistry::PluginIds() const {
    return plugin_ids_;
}

const std::vector<std::string>&
KSMFrameworkPluginRegistry::Commands() const {
    return command_names_;
}

const std::vector<std::string>&
KSMFrameworkPluginRegistry::Permissions() const {
    return permissions_;
}

const std::vector<std::string>&
KSMFrameworkPluginRegistry::Events() const {
    return events_;
}
