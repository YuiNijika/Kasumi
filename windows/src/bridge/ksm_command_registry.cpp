#include "ksm_command_registry.h"

#include "../generated/contract.h"

const std::vector<std::string>& KSMCommandRegistry::SupportedCommands() {
    return KSMGeneratedContract::WindowsCommands();
}
