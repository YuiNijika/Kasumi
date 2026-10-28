#pragma once

#include <string>
#include <vector>

class KSMCommandRegistry {
public:
    static const std::vector<std::string>& SupportedCommands();
};
