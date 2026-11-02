#include "ksm_store_plugin.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

#include "../commands/ksm_filesystem.h"
#include "../platform/ksm_host_services.h"

namespace {

using Json = nlohmann::json;

constexpr char kStorePath[] = "app://data/.kasumi/store.json";
constexpr std::size_t kMaxKeyBytes = 256;
constexpr std::size_t kMaxValueBytes = 1024 * 1024;
constexpr std::size_t kMaxStoreBytes = 16 * 1024 * 1024;

std::mutex& StoreMutex() {
    static std::mutex mutex;
    return mutex;
}

std::shared_ptr<KSMFileSystem> RequireFileSystem(
    const KSMPluginContext& context
) {
    if (context.file_system) {
        return context.file_system;
    }
    throw KSMHostException(
        "NOT_SUPPORTED",
        "The filesystem is not configured."
    );
}

void RequirePermission(const KSMPluginContext& context) {
    if (context.has_permission && context.has_permission("store")) {
        return;
    }
    throw KSMHostException(
        "PERMISSION_DENIED",
        "The requested permission was denied."
    );
}

const std::string& KeyArg(const Json& args) {
    if (
        !args.contains("key") ||
        !args.at("key").is_string() ||
        args.at("key").get_ref<const std::string&>().empty() ||
        args.at("key").get_ref<const std::string&>().size() > kMaxKeyBytes
    ) {
        throw KSMHostException(
            "INVALID_ARGS",
            "key must contain between 1 and 256 bytes."
        );
    }
    return args.at("key").get_ref<const std::string&>();
}

} // namespace

const std::string& KSMStorePlugin::Id() const {
    static const std::string id = "store";
    return id;
}

const std::vector<std::string>& KSMStorePlugin::Commands() const {
    static const std::vector<std::string> commands = {
        "store.get",
        "store.set",
        "store.delete",
        "store.clear",
        "store.keys"
    };
    return commands;
}

const std::vector<std::string>& KSMStorePlugin::Permissions() const {
    static const std::vector<std::string> permissions = {"store"};
    return permissions;
}

Json KSMStorePlugin::Load(const KSMPluginContext& context) const {
    const auto file_system = RequireFileSystem(context);
    if (!file_system->Exists(kStorePath)) {
        return Json::object();
    }
    const auto text = file_system->ReadText(kStorePath);
    if (!text) {
        throw KSMHostException("IO_ERROR", "The store could not be read.");
    }
    try {
        Json root = Json::parse(*text);
        if (!root.is_object()) {
            throw KSMHostException(
                "IO_ERROR",
                "The store file does not contain a JSON object."
            );
        }
        return root;
    } catch (const KSMHostException&) {
        throw;
    } catch (const Json::exception&) {
        throw KSMHostException(
            "IO_ERROR",
            "The store file contains invalid JSON."
        );
    }
}

void KSMStorePlugin::Save(
    const KSMPluginContext& context,
    const Json& root
) const {
    const auto file_system = RequireFileSystem(context);
    const std::string content = root.dump();
    if (content.size() > kMaxStoreBytes) {
        throw KSMHostException("IO_ERROR", "The store exceeds 16 MiB.");
    }
    if (!file_system->WriteText(kStorePath, content)) {
        throw KSMHostException("IO_ERROR", "The store could not be written.");
    }
}

Json KSMStorePlugin::Handle(
    const std::string& command,
    const Json& args,
    const KSMPluginContext& context
) {
    RequirePermission(context);
    std::lock_guard lock(StoreMutex());

    if (command == "store.get") {
        const std::string& key = KeyArg(args);
        const Json root = Load(context);
        const auto iterator = root.find(key);
        if (iterator == root.end()) {
            return Json{{"found", false}, {"value", nullptr}};
        }
        return Json{{"found", true}, {"value", *iterator}};
    }

    if (command == "store.keys") {
        Json root = Load(context);
        std::vector<std::string> keys;
        keys.reserve(root.size());
        for (const auto& item : root.items()) {
            keys.push_back(item.key());
        }
        std::sort(keys.begin(), keys.end());
        return Json{{"keys", keys}};
    }

    if (command == "store.clear") {
        const auto file_system = RequireFileSystem(context);
        if (
            file_system->Exists(kStorePath) &&
            !file_system->Remove(kStorePath)
        ) {
            throw KSMHostException(
                "IO_ERROR",
                "The store could not be cleared."
            );
        }
        return Json{{"cleared", true}};
    }

    const std::string& key = KeyArg(args);
    Json root = Load(context);
    if (command == "store.delete") {
        const bool deleted = root.erase(key) > 0;
        if (deleted) {
            Save(context, root);
        }
        return Json{{"deleted", deleted}};
    }

    if (!args.contains("value")) {
        throw KSMHostException("INVALID_ARGS", "value is required.");
    }
    const Json& value = args.at("value");
    if (value.dump().size() > kMaxValueBytes) {
        throw KSMHostException(
            "INVALID_ARGS",
            "value must not exceed 1 MiB."
        );
    }
    root[key] = value;
    Save(context, root);
    return Json{{"updated", true}};
}
