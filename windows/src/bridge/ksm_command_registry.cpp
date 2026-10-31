#include "ksm_command_registry.h"

#include <algorithm>
#include <set>

#include "../generated/contract.h"

std::vector<std::string> KSMCommandRegistry::SupportedCommands(
    const std::vector<std::string>& requested
) {
    const auto& platform_commands = KSMGeneratedContract::WindowsCommands();
    if (requested.empty()) {
        return platform_commands;
    }
    const std::set<std::string> allowed(requested.begin(), requested.end());
    std::vector<std::string> result;
    for (const auto& command : platform_commands) {
        if (allowed.find(command) != allowed.end()) {
            result.push_back(command);
        }
    }
    return result;
}
