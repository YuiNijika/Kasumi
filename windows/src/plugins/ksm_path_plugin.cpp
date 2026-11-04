#include "ksm_path_plugin.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "../platform/ksm_host_services.h"

namespace {

using Json = nlohmann::json;

constexpr std::size_t kAppSchemeLength = 6;
const std::vector<std::string> kRoots = {
    "app://data",
    "app://cache",
    "app://tmp",
    "app://picked",
    "app://res",
    "app://assets"
};

bool IsRoot(const std::string& path) {
    return std::find(kRoots.begin(), kRoots.end(), path) != kRoots.end();
}

bool IsSafeSegment(const std::string& segment) {
    return !segment.empty() &&
        segment != "." &&
        segment != ".." &&
        segment.find('\0') == std::string::npos &&
        segment.find('\\') == std::string::npos &&
        segment.find(':') == std::string::npos &&
        segment.find('~') == std::string::npos;
}

std::optional<std::string> Normalize(const std::string& raw) {
    if (raw.empty() || raw.find('\0') != std::string::npos) {
        return std::nullopt;
    }
    const auto root_iterator = std::find_if(
        kRoots.begin(),
        kRoots.end(),
        [&raw](const std::string& root) {
            return raw == root || raw.starts_with(root + "/");
        }
    );
    if (root_iterator == kRoots.end()) {
        return std::nullopt;
    }
    const std::string& root = *root_iterator;
    if (raw.size() == root.size() || raw == root + "/") {
        return root;
    }

    std::string remaining = raw.substr(root.size() + 1);
    while (!remaining.empty() && remaining.back() == '/') {
        remaining.pop_back();
    }
    if (remaining.empty()) {
        return root;
    }

    std::string normalized = root;
    std::size_t start = 0;
    while (start <= remaining.size()) {
        const auto end = remaining.find('/', start);
        const std::string segment = remaining.substr(
            start,
            end == std::string::npos
                ? std::string::npos
                : end - start
        );
        if (!IsSafeSegment(segment)) {
            return std::nullopt;
        }
        normalized += "/" + segment;
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    if (root == "app://picked" && normalized.find('/', root.size() + 1) !=
            std::string::npos) {
        return std::nullopt;
    }
    return normalized;
}

const std::string& StringArg(
    const Json& args,
    const char* field
) {
    if (!args.contains(field) || !args.at(field).is_string()) {
        throw KSMHostException(
            "INVALID_ARGS",
            std::string(field) + " is required."
        );
    }
    return args.at(field).get_ref<const std::string&>();
}

std::string NormalizeArg(const Json& args) {
    const std::string& path = StringArg(args, "path");
    const auto normalized = Normalize(path);
    if (!normalized) {
        throw KSMHostException(
            "INVALID_ARGS",
            "path must be a Kasumi virtual path without traversal."
        );
    }
    return *normalized;
}

Json Dirs() {
    return Json{
        {"data", "app://data"},
        {"cache", "app://cache"},
        {"temp", "app://tmp"},
        {"resource", "app://res"},
        {"asset", "app://assets"},
        {"home", "app://data"},
        {"config", "app://data/config"},
        {"log", "app://data/log"},
        {"runtime", "app://cache"},
        {"executable", "app://assets"},
        {"desktop", ""},
        {"download", ""}
    };
}

Json Join(const Json& args) {
    const std::string& base = StringArg(args, "base");
    const auto normalized = Normalize(base);
    if (!normalized) {
        throw KSMHostException(
            "INVALID_ARGS",
            "base must be a Kasumi virtual path without traversal."
        );
    }
    if (!args.contains("segments") || !args.at("segments").is_array()) {
        throw KSMHostException("INVALID_ARGS", "segments must be an array.");
    }
    std::string result = *normalized;
    for (const auto& value : args.at("segments")) {
        if (!value.is_string()) {
            throw KSMHostException(
                "INVALID_ARGS",
                "segments must contain strings."
            );
        }
        const std::string& fragment = value.get_ref<const std::string&>();
        if (
            fragment.empty() ||
            fragment.starts_with('/') ||
            fragment.find('\\') != std::string::npos ||
            fragment.find(':') != std::string::npos ||
            fragment.find('\0') != std::string::npos ||
            fragment.find('~') != std::string::npos
        ) {
            throw KSMHostException(
                "INVALID_ARGS",
                "segments must be relative virtual path fragments."
            );
        }
        std::size_t start = 0;
        while (start <= fragment.size()) {
            const auto end = fragment.find('/', start);
            const std::string segment = fragment.substr(
                start,
                end == std::string::npos
                    ? std::string::npos
                    : end - start
            );
            if (!IsSafeSegment(segment)) {
                throw KSMHostException(
                    "INVALID_ARGS",
                    "segments contain an invalid path component."
                );
            }
            result += "/" + segment;
            if (end == std::string::npos) {
                break;
            }
            start = end + 1;
        }
    }
    const auto final_path = Normalize(result);
    if (!final_path) {
        throw KSMHostException("INVALID_ARGS", "The joined path is invalid.");
    }
    return Json{{"path", *final_path}};
}

Json Dirname(const Json& args) {
    const std::string path = NormalizeArg(args);
    const auto separator = path.rfind('/');
    if (
        separator == std::string::npos ||
        separator <= kAppSchemeLength + 1
    ) {
        return Json{{"path", path.substr(0, path.find('/', kAppSchemeLength + 1))}};
    }
    return Json{{"path", path.substr(0, separator)}};
}

Json Basename(const Json& args) {
    const std::string path = NormalizeArg(args);
    const auto separator = path.rfind('/');
    std::string name = separator == std::string::npos
        ? path
        : path.substr(separator + 1);
    if (separator == std::string::npos || separator <= kAppSchemeLength + 1) {
        name.clear();
    }
    if (args.contains("suffix")) {
        if (!args.at("suffix").is_string()) {
            throw KSMHostException("INVALID_ARGS", "suffix must be a string.");
        }
        const std::string& suffix =
            args.at("suffix").get_ref<const std::string&>();
        if (
            !suffix.empty() &&
            name.size() > suffix.size() &&
            name.ends_with(suffix)
        ) {
            name.resize(name.size() - suffix.size());
        }
    }
    return Json{{"name", name}};
}

Json Extname(const Json& args) {
    const std::string path = NormalizeArg(args);
    const auto separator = path.rfind('/');
    const auto dot = path.rfind('.');
    const std::size_t name_start = separator == std::string::npos
        ? kAppSchemeLength
        : separator + 1;
    if (
        dot == std::string::npos ||
        dot <= name_start ||
        (separator != std::string::npos && dot < separator)
    ) {
        return Json{{"extension", ""}};
    }
    return Json{{"extension", path.substr(dot)}};
}

Json IsAbsolute(const Json& args) {
    const std::string& path = StringArg(args, "path");
    return Json{{"absolute", Normalize(path).has_value()}};
}

} // namespace

const std::string& KSMPathPlugin::Id() const {
    static const std::string id = "path";
    return id;
}

const std::vector<std::string>& KSMPathPlugin::Commands() const {
    static const std::vector<std::string> commands = {
        "path.dirs",
        "path.join",
        "path.normalize",
        "path.dirname",
        "path.basename",
        "path.extname",
        "path.isAbsolute"
    };
    return commands;
}

Json KSMPathPlugin::Handle(
    const std::string& command,
    const Json& args,
    const KSMPluginContext&
) {
    if (command == "path.dirs") {
        return Dirs();
    }
    if (command == "path.join") {
        return Join(args);
    }
    if (command == "path.normalize") {
        return Json{{"path", NormalizeArg(args)}};
    }
    if (command == "path.dirname") {
        return Dirname(args);
    }
    if (command == "path.basename") {
        return Basename(args);
    }
    if (command == "path.extname") {
        return Extname(args);
    }
    return IsAbsolute(args);
}
