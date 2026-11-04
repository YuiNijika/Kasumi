#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace ksm_contract_detail {

using Json = nlohmann::json;

inline const Json& ArgSchemas() {
    static const Json schemas = Json::parse(R"KSMSCHEMA({
  "ping": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "capabilities.query": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "app.getInfo": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "device.getInfo": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "fs.readText": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "fs.writeText": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      },
      "content": {
        "type": "string",
        "maxBytes": 16777216
      }
    },
    "required": [
      "path",
      "content"
    ],
    "additionalProperties": false
  },
  "fs.exists": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "fs.remove": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "fs.readDir": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "fs.mkdir": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      },
      "recursive": {
        "type": "boolean"
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "fs.stat": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "fs.appendText": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      },
      "content": {
        "type": "string",
        "maxBytes": 16777216
      }
    },
    "required": [
      "path",
      "content"
    ],
    "additionalProperties": false
  },
  "fs.copy": {
    "type": "object",
    "properties": {
      "from": {
        "type": "string",
        "minLength": 1
      },
      "to": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "from",
      "to"
    ],
    "additionalProperties": false
  },
  "fs.move": {
    "type": "object",
    "properties": {
      "from": {
        "type": "string",
        "minLength": 1
      },
      "to": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "from",
      "to"
    ],
    "additionalProperties": false
  },
  "fs.readBytes": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "fs.writeBytes": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      },
      "base64": {
        "type": "string",
        "maxBytes": 33554432
      }
    },
    "required": [
      "path",
      "base64"
    ],
    "additionalProperties": false
  },
  "dialog.message": {
    "type": "object",
    "properties": {
      "title": {
        "type": "string"
      },
      "message": {
        "type": "string"
      }
    },
    "required": [
      "title",
      "message"
    ],
    "additionalProperties": false
  },
  "dialog.confirm": {
    "type": "object",
    "properties": {
      "title": {
        "type": "string"
      },
      "message": {
        "type": "string"
      }
    },
    "required": [
      "title",
      "message"
    ],
    "additionalProperties": false
  },
  "dialog.openFile": {
    "type": "object",
    "properties": {
      "filters": {
        "type": "array",
        "items": {
          "type": "string"
        }
      }
    },
    "required": [],
    "additionalProperties": false
  },
  "dialog.saveFile": {
    "type": "object",
    "properties": {
      "suggestedName": {
        "type": "string"
      }
    },
    "required": [],
    "additionalProperties": false
  },
  "clipboard.readText": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "clipboard.writeText": {
    "type": "object",
    "properties": {
      "text": {
        "type": "string"
      }
    },
    "required": [
      "text"
    ],
    "additionalProperties": false
  },
  "notification.show": {
    "type": "object",
    "properties": {
      "title": {
        "type": "string"
      },
      "body": {
        "type": "string"
      }
    },
    "required": [
      "title",
      "body"
    ],
    "additionalProperties": false
  },
  "permission.request": {
    "type": "object",
    "properties": {
      "permission": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "permission"
    ],
    "additionalProperties": false
  },
  "event.subscribe": {
    "type": "object",
    "properties": {
      "event": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "event"
    ],
    "additionalProperties": false
  },
  "event.unsubscribe": {
    "type": "object",
    "properties": {
      "event": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "event"
    ],
    "additionalProperties": false
  },
  "window.setTitle": {
    "type": "object",
    "properties": {
      "title": {
        "type": "string"
      }
    },
    "required": [
      "title"
    ],
    "additionalProperties": false
  },
  "window.minimize": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "window.maximize": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "window.setSize": {
    "type": "object",
    "properties": {
      "width": {
        "type": "integer"
      },
      "height": {
        "type": "integer"
      }
    },
    "required": [
      "width",
      "height"
    ],
    "additionalProperties": false
  },
  "window.close": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "shell.openExternal": {
    "type": "object",
    "properties": {
      "url": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "url"
    ],
    "additionalProperties": false
  },
  "shell.openPath": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "shell.showInFolder": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "app.singleInstance.state": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "app.singleInstance.focus": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "tray.show": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "tray.hide": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "globalShortcut.register": {
    "type": "object",
    "properties": {
      "shortcut": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 256
      }
    },
    "required": [
      "shortcut"
    ],
    "additionalProperties": false
  },
  "globalShortcut.unregister": {
    "type": "object",
    "properties": {
      "shortcut": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 256
      }
    },
    "required": [
      "shortcut"
    ],
    "additionalProperties": false
  },
  "updater.check": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "updater.download": {
    "type": "object",
    "properties": {
      "version": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 128
      }
    },
    "required": [
      "version"
    ],
    "additionalProperties": false
  },
  "http.download": {
    "type": "object",
    "properties": {
      "url": {
        "type": "string",
        "minLength": 1
      },
      "path": {
        "type": "string",
        "minLength": 1
      },
      "overwrite": {
        "type": "boolean"
      }
    },
    "required": [
      "url",
      "path"
    ],
    "additionalProperties": false
  },
  "sql.drivers": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "sql.open": {
    "type": "object",
    "properties": {
      "driver": {
        "type": "string",
        "enum": [
          "sqlite",
          "mysql",
          "postgres",
          "mongodb"
        ]
      },
      "url": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 4096
      }
    },
    "required": [
      "driver",
      "url"
    ],
    "additionalProperties": false
  },
  "sql.query": {
    "type": "object",
    "properties": {
      "connectionId": {
        "type": "string",
        "minLength": 1
      },
      "statement": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 1048576
      },
      "params": {
        "type": "array",
        "items": {
          "type": [
            "string",
            "number",
            "boolean",
            "null"
          ]
        }
      }
    },
    "required": [
      "connectionId",
      "statement"
    ],
    "additionalProperties": false
  },
  "sql.execute": {
    "type": "object",
    "properties": {
      "connectionId": {
        "type": "string",
        "minLength": 1
      },
      "statement": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 1048576
      },
      "params": {
        "type": "array",
        "items": {
          "type": [
            "string",
            "number",
            "boolean",
            "null"
          ]
        }
      }
    },
    "required": [
      "connectionId",
      "statement"
    ],
    "additionalProperties": false
  },
  "sql.close": {
    "type": "object",
    "properties": {
      "connectionId": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "connectionId"
    ],
    "additionalProperties": false
  },
  "path.dirs": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "path.join": {
    "type": "object",
    "properties": {
      "base": {
        "type": "string",
        "minLength": 1
      },
      "segments": {
        "type": "array",
        "items": {
          "type": "string",
          "minLength": 1,
          "maxBytes": 256
        }
      }
    },
    "required": [
      "base",
      "segments"
    ],
    "additionalProperties": false
  },
  "path.normalize": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 8192
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "path.dirname": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 8192
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "path.basename": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 8192
      },
      "suffix": {
        "type": "string",
        "maxBytes": 256
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "path.extname": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 8192
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "path.isAbsolute": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 8192
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "store.get": {
    "type": "object",
    "properties": {
      "key": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 256
      }
    },
    "required": [
      "key"
    ],
    "additionalProperties": false
  },
  "store.set": {
    "type": "object",
    "properties": {
      "key": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 256
      },
      "value": {
        "type": [
          "object",
          "array",
          "string",
          "number",
          "boolean",
          "null"
        ]
      }
    },
    "required": [
      "key",
      "value"
    ],
    "additionalProperties": false
  },
  "store.delete": {
    "type": "object",
    "properties": {
      "key": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 256
      }
    },
    "required": [
      "key"
    ],
    "additionalProperties": false
  },
  "store.clear": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "store.keys": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "os.getInfo": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "log.write": {
    "type": "object",
    "properties": {
      "level": {
        "type": "string",
        "enum": [
          "trace",
          "debug",
          "info",
          "warn",
          "error"
        ]
      },
      "message": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 65536
      },
      "target": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 128
      }
    },
    "required": [
      "level",
      "message"
    ],
    "additionalProperties": false
  },
  "app.exit": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  }
})KSMSCHEMA");
    return schemas;
}

inline bool MatchesType(const Json& value, const std::string& type) {
    if (type == "object") return value.is_object();
    if (type == "array") return value.is_array();
    if (type == "string") return value.is_string();
    if (type == "number") return value.is_number();
    if (type == "integer") return value.is_number_integer() || value.is_number_unsigned();
    if (type == "boolean") return value.is_boolean();
    if (type == "null") return value.is_null();
    return false;
}

inline bool SameValue(const Json& left, const Json& right) {
    return left == right;
}

inline bool ValidateValue(const Json& value, const Json& schema) {
    if (!schema.contains("type")) return false;
    const Json& type = schema.at("type");
    bool matches = false;
    if (type.is_string()) {
        matches = MatchesType(value, type.get<std::string>());
    } else if (type.is_array()) {
        for (const auto& item : type) {
            if (item.is_string() && MatchesType(value, item.get<std::string>())) {
                matches = true;
                break;
            }
        }
    }
    if (!matches) return false;

    if (schema.contains("enum")) {
        bool found = false;
        for (const auto& candidate : schema.at("enum")) {
            if (SameValue(candidate, value)) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    if (schema.contains("const") && !SameValue(schema.at("const"), value)) {
        return false;
    }

    if (value.is_object()) {
        const Json properties = schema.value("properties", Json::object());
        if (schema.contains("required")) {
            for (const auto& field : schema.at("required")) {
                if (!value.contains(field.get<std::string>())) return false;
            }
        }
        if (schema.value("additionalProperties", true) == false) {
            for (const auto& [field, unused] : value.items()) {
                (void)unused;
                if (!properties.contains(field)) return false;
            }
        }
        for (const auto& [field, property_schema] : properties.items()) {
            if (value.contains(field) && !ValidateValue(value.at(field), property_schema)) {
                return false;
            }
        }
    }

    if (value.is_array() && schema.contains("items")) {
        for (const auto& item : value) {
            if (!ValidateValue(item, schema.at("items"))) return false;
        }
    }

    if (value.is_string()) {
        const std::string text = value.get<std::string>();
        if (schema.contains("minLength") && text.size() < schema.at("minLength").get<std::size_t>()) {
            return false;
        }
        if (schema.contains("maxBytes") && text.size() > schema.at("maxBytes").get<std::size_t>()) {
            return false;
        }
    }
    return true;
}

}  // namespace ksm_contract_detail

class KSMGeneratedContract {
public:
    static const std::vector<std::string>& Commands();
    static const std::vector<std::string>& WindowsCommands();
    static const std::vector<std::string>& AndroidCommands();
    static const std::vector<std::string>& Events();
    static const std::vector<std::string>& Permissions();
    static const std::vector<std::string>& Plugins();
    static const nlohmann::json& PluginMetadata();
    static const nlohmann::json& PluginCommands();
    static const nlohmann::json& CommandPermissions();
    static bool ValidateArgs(const std::string& command, const nlohmann::json& args);
};

inline const std::vector<std::string>& KSMGeneratedContract::Commands() {
    static const std::vector<std::string> values = {
        "ping", "capabilities.query", "app.getInfo", "device.getInfo", "fs.readText", "fs.writeText", "fs.exists", "fs.remove", "fs.readDir", "fs.mkdir", "fs.stat", "fs.appendText", "fs.copy", "fs.move", "fs.readBytes", "fs.writeBytes", "dialog.message", "dialog.confirm", "dialog.openFile", "dialog.saveFile", "clipboard.readText", "clipboard.writeText", "notification.show", "permission.request", "event.subscribe", "event.unsubscribe", "window.setTitle", "window.minimize", "window.maximize", "window.setSize", "window.close", "shell.openExternal", "shell.openPath", "shell.showInFolder", "app.singleInstance.state", "app.singleInstance.focus", "tray.show", "tray.hide", "globalShortcut.register", "globalShortcut.unregister", "updater.check", "updater.download", "http.download", "sql.drivers", "sql.open", "sql.query", "sql.execute", "sql.close", "path.dirs", "path.join", "path.normalize", "path.dirname", "path.basename", "path.extname", "path.isAbsolute", "store.get", "store.set", "store.delete", "store.clear", "store.keys", "os.getInfo", "log.write", "app.exit"
    };
    return values;
}

inline const std::vector<std::string>& KSMGeneratedContract::WindowsCommands() {
    static const std::vector<std::string> values = {
        "ping", "capabilities.query", "app.getInfo", "fs.readText", "fs.writeText", "fs.exists", "fs.remove", "fs.readDir", "fs.mkdir", "fs.stat", "fs.appendText", "fs.copy", "fs.move", "fs.readBytes", "fs.writeBytes", "dialog.message", "dialog.confirm", "dialog.openFile", "dialog.saveFile", "clipboard.readText", "clipboard.writeText", "notification.show", "permission.request", "event.subscribe", "event.unsubscribe", "window.setTitle", "window.minimize", "window.maximize", "window.setSize", "window.close", "shell.openExternal", "shell.openPath", "shell.showInFolder", "app.singleInstance.state", "app.singleInstance.focus", "tray.show", "tray.hide", "globalShortcut.register", "globalShortcut.unregister", "updater.check", "updater.download", "http.download", "sql.drivers", "sql.open", "sql.query", "sql.execute", "sql.close", "path.dirs", "path.join", "path.normalize", "path.dirname", "path.basename", "path.extname", "path.isAbsolute", "store.get", "store.set", "store.delete", "store.clear", "store.keys", "os.getInfo", "log.write", "app.exit"
    };
    return values;
}

inline const std::vector<std::string>& KSMGeneratedContract::AndroidCommands() {
    static const std::vector<std::string> values = {
        "ping", "capabilities.query", "app.getInfo", "device.getInfo", "fs.readText", "fs.writeText", "fs.exists", "fs.remove", "fs.readDir", "fs.mkdir", "fs.stat", "fs.appendText", "fs.copy", "fs.move", "fs.readBytes", "fs.writeBytes", "dialog.message", "dialog.confirm", "dialog.openFile", "dialog.saveFile", "clipboard.readText", "clipboard.writeText", "notification.show", "permission.request", "event.subscribe", "event.unsubscribe", "window.close", "shell.openExternal", "shell.openPath", "shell.showInFolder", "app.singleInstance.state", "app.singleInstance.focus", "updater.check", "updater.download", "http.download", "sql.drivers", "sql.open", "sql.query", "sql.execute", "sql.close", "path.dirs", "path.join", "path.normalize", "path.dirname", "path.basename", "path.extname", "path.isAbsolute", "store.get", "store.set", "store.delete", "store.clear", "store.keys", "os.getInfo", "log.write", "app.exit"
    };
    return values;
}

inline const std::vector<std::string>& KSMGeneratedContract::Events() {
    static const std::vector<std::string> values = {
        "webview.navigated", "webview.reloaded", "window.resized"
    };
    return values;
}

inline const std::vector<std::string>& KSMGeneratedContract::Permissions() {
    static const std::vector<std::string> values = {
        "fs.read", "fs.write", "fs.remove", "dialog", "clipboard.read", "clipboard.write", "notification.show", "window.control", "http.download", "shell.open", "device.info", "sql", "store", "os.info", "log.write", "updater", "globalShortcut", "tray"
    };
    return values;
}

inline const std::vector<std::string>& KSMGeneratedContract::Plugins() {
    static const std::vector<std::string> values = {
        "core", "fs", "dialog", "clipboard", "notification", "window", "http", "shell", "device", "sql", "path", "store", "os", "log", "appInstance", "updater", "globalShortcut", "tray"
    };
    return values;
}

inline const nlohmann::json& KSMGeneratedContract::PluginMetadata() {
    static const nlohmann::json value = nlohmann::json::parse(
        R"KSMMETADATA({
  "core": {
    "name": "Core",
    "version": "1.0.0",
    "description": "Application lifecycle, permissions, events and capability discovery.",
    "frontendPackage": "kasumi-bridge",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "window.control"
    ],
    "defaultPermissions": [
      "window.control"
    ],
    "commands": [
      "ping",
      "capabilities.query",
      "app.getInfo",
      "app.exit",
      "permission.request",
      "event.subscribe",
      "event.unsubscribe"
    ]
  },
  "fs": {
    "name": "Filesystem",
    "version": "1.0.0",
    "description": "Sandboxed virtual filesystem access.",
    "frontendPackage": "kasumi-plugin-fs",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "fs.read",
      "fs.write",
      "fs.remove"
    ],
    "defaultPermissions": [
      "fs.read",
      "fs.write",
      "fs.remove"
    ],
    "commands": [
      "fs.readText",
      "fs.writeText",
      "fs.appendText",
      "fs.copy",
      "fs.move",
      "fs.readBytes",
      "fs.writeBytes",
      "fs.exists",
      "fs.remove",
      "fs.readDir",
      "fs.mkdir",
      "fs.stat"
    ]
  },
  "dialog": {
    "name": "Dialog",
    "version": "1.0.0",
    "description": "Native message, confirmation and file picker dialogs.",
    "frontendPackage": "kasumi-plugin-dialog",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "dialog"
    ],
    "defaultPermissions": [
      "dialog"
    ],
    "commands": [
      "dialog.message",
      "dialog.confirm",
      "dialog.openFile",
      "dialog.saveFile"
    ]
  },
  "clipboard": {
    "name": "Clipboard",
    "version": "1.0.0",
    "description": "System clipboard text access.",
    "frontendPackage": "kasumi-plugin-clipboard",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "clipboard.read",
      "clipboard.write"
    ],
    "defaultPermissions": [
      "clipboard.read",
      "clipboard.write"
    ],
    "commands": [
      "clipboard.readText",
      "clipboard.writeText"
    ]
  },
  "notification": {
    "name": "Notification",
    "version": "1.0.0",
    "description": "Native system notifications.",
    "frontendPackage": "kasumi-plugin-notification",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "notification.show"
    ],
    "defaultPermissions": [
      "notification.show"
    ],
    "commands": [
      "notification.show"
    ]
  },
  "window": {
    "name": "Window",
    "version": "1.0.0",
    "description": "Window title, size, lifecycle and application exit controls.",
    "frontendPackage": "kasumi-plugin-window",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "window.control"
    ],
    "defaultPermissions": [
      "window.control"
    ],
    "commands": [
      "window.setTitle",
      "window.minimize",
      "window.maximize",
      "window.setSize",
      "window.close"
    ]
  },
  "http": {
    "name": "HTTP",
    "version": "1.0.0",
    "description": "HTTPS downloads into the virtual filesystem.",
    "frontendPackage": "kasumi-plugin-http",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "http.download"
    ],
    "defaultPermissions": [
      "http.download"
    ],
    "commands": [
      "http.download"
    ]
  },
  "shell": {
    "name": "Shell",
    "version": "1.4.0",
    "description": "Allowlisted external URL launching.",
    "frontendPackage": "kasumi-plugin-shell",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "shell.open"
    ],
    "defaultPermissions": [
      "shell.open"
    ],
    "commands": [
      "shell.openExternal",
      "shell.openPath",
      "shell.showInFolder"
    ]
  },
  "device": {
    "name": "Device",
    "version": "1.0.0",
    "description": "Non-identifying Android device information.",
    "frontendPackage": "kasumi-plugin-device",
    "platforms": [
      "android"
    ],
    "permissions": [
      "device.info"
    ],
    "defaultPermissions": [
      "device.info"
    ],
    "commands": [
      "device.getInfo"
    ]
  },
  "sql": {
    "name": "SQL",
    "version": "1.2.0",
    "description": "Database connections and parameterised SQL statements.",
    "frontendPackage": "kasumi-plugin-sql",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "sql"
    ],
    "defaultPermissions": [
      "sql"
    ],
    "commands": [
      "sql.drivers",
      "sql.open",
      "sql.query",
      "sql.execute",
      "sql.close"
    ]
  },
  "path": {
    "name": "Path",
    "version": "1.3.0",
    "description": "Virtual application path inspection and construction.",
    "frontendPackage": "kasumi-plugin-path",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [],
    "defaultPermissions": [],
    "commands": [
      "path.dirs",
      "path.join",
      "path.normalize",
      "path.dirname",
      "path.basename",
      "path.extname",
      "path.isAbsolute"
    ]
  },
  "store": {
    "name": "Store",
    "version": "1.3.0",
    "description": "Persistent JSON key-value storage inside application data.",
    "frontendPackage": "kasumi-plugin-store",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "store"
    ],
    "defaultPermissions": [
      "store"
    ],
    "commands": [
      "store.get",
      "store.set",
      "store.delete",
      "store.clear",
      "store.keys"
    ]
  },
  "os": {
    "name": "OS",
    "version": "1.3.0",
    "description": "Non-identifying operating system and runtime information.",
    "frontendPackage": "kasumi-plugin-os",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "os.info"
    ],
    "defaultPermissions": [
      "os.info"
    ],
    "commands": [
      "os.getInfo"
    ]
  },
  "log": {
    "name": "Log",
    "version": "1.3.0",
    "description": "Platform logger access with bounded messages.",
    "frontendPackage": "kasumi-plugin-log",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "log.write"
    ],
    "defaultPermissions": [
      "log.write"
    ],
    "commands": [
      "log.write"
    ]
  },
  "appInstance": {
    "name": "App Instance",
    "version": "1.4.0",
    "description": "Single instance state and focus coordination.",
    "frontendPackage": "kasumi-bridge",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [],
    "defaultPermissions": [],
    "commands": [
      "app.singleInstance.state",
      "app.singleInstance.focus"
    ]
  },
  "updater": {
    "name": "Updater",
    "version": "1.4.0",
    "description": "Check and download application update packages.",
    "frontendPackage": "kasumi-plugin-updater",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "updater"
    ],
    "defaultPermissions": [
      "updater"
    ],
    "commands": [
      "updater.check",
      "updater.download"
    ]
  },
  "globalShortcut": {
    "name": "Global Shortcut",
    "version": "1.4.0",
    "description": "Register and unregister global keyboard shortcuts.",
    "frontendPackage": "kasumi-plugin-global-shortcut",
    "platforms": [
      "windows"
    ],
    "permissions": [
      "globalShortcut"
    ],
    "defaultPermissions": [
      "globalShortcut"
    ],
    "commands": [
      "globalShortcut.register",
      "globalShortcut.unregister"
    ]
  },
  "tray": {
    "name": "Tray",
    "version": "1.4.0",
    "description": "Show and hide the system tray icon.",
    "frontendPackage": "kasumi-plugin-tray",
    "platforms": [
      "windows"
    ],
    "permissions": [
      "tray"
    ],
    "defaultPermissions": [
      "tray"
    ],
    "commands": [
      "tray.show",
      "tray.hide"
    ]
  }
})KSMMETADATA"
    );
    return value;
}

inline const nlohmann::json& KSMGeneratedContract::PluginCommands() {
    static const nlohmann::json value = nlohmann::json::parse(
        R"KSMPLUGINS({
  "core": [
    "ping",
    "capabilities.query",
    "app.getInfo",
    "app.exit",
    "permission.request",
    "event.subscribe",
    "event.unsubscribe"
  ],
  "fs": [
    "fs.readText",
    "fs.writeText",
    "fs.appendText",
    "fs.copy",
    "fs.move",
    "fs.readBytes",
    "fs.writeBytes",
    "fs.exists",
    "fs.remove",
    "fs.readDir",
    "fs.mkdir",
    "fs.stat"
  ],
  "dialog": [
    "dialog.message",
    "dialog.confirm",
    "dialog.openFile",
    "dialog.saveFile"
  ],
  "clipboard": [
    "clipboard.readText",
    "clipboard.writeText"
  ],
  "notification": [
    "notification.show"
  ],
  "window": [
    "window.setTitle",
    "window.minimize",
    "window.maximize",
    "window.setSize",
    "window.close"
  ],
  "http": [
    "http.download"
  ],
  "shell": [
    "shell.openExternal",
    "shell.openPath",
    "shell.showInFolder"
  ],
  "device": [
    "device.getInfo"
  ],
  "sql": [
    "sql.drivers",
    "sql.open",
    "sql.query",
    "sql.execute",
    "sql.close"
  ],
  "path": [
    "path.dirs",
    "path.join",
    "path.normalize",
    "path.dirname",
    "path.basename",
    "path.extname",
    "path.isAbsolute"
  ],
  "store": [
    "store.get",
    "store.set",
    "store.delete",
    "store.clear",
    "store.keys"
  ],
  "os": [
    "os.getInfo"
  ],
  "log": [
    "log.write"
  ],
  "appInstance": [
    "app.singleInstance.state",
    "app.singleInstance.focus"
  ],
  "updater": [
    "updater.check",
    "updater.download"
  ],
  "globalShortcut": [
    "globalShortcut.register",
    "globalShortcut.unregister"
  ],
  "tray": [
    "tray.show",
    "tray.hide"
  ]
})KSMPLUGINS"
    );
    return value;
}

inline const nlohmann::json& KSMGeneratedContract::CommandPermissions() {
    static const nlohmann::json value = nlohmann::json::parse(
        R"KSMPERMISSIONS({
  "ping": [],
  "capabilities.query": [],
  "app.getInfo": [],
  "device.getInfo": [
    "device.info"
  ],
  "fs.readText": [
    "fs.read"
  ],
  "fs.writeText": [
    "fs.write"
  ],
  "fs.exists": [
    "fs.read"
  ],
  "fs.remove": [
    "fs.remove"
  ],
  "fs.readDir": [
    "fs.read"
  ],
  "fs.mkdir": [
    "fs.write"
  ],
  "fs.stat": [
    "fs.read"
  ],
  "fs.appendText": [
    "fs.write"
  ],
  "fs.copy": [
    "fs.read",
    "fs.write"
  ],
  "fs.move": [
    "fs.write"
  ],
  "fs.readBytes": [
    "fs.read"
  ],
  "fs.writeBytes": [
    "fs.write"
  ],
  "dialog.message": [
    "dialog"
  ],
  "dialog.confirm": [
    "dialog"
  ],
  "dialog.openFile": [
    "dialog"
  ],
  "dialog.saveFile": [
    "dialog"
  ],
  "clipboard.readText": [
    "clipboard.read"
  ],
  "clipboard.writeText": [
    "clipboard.write"
  ],
  "notification.show": [
    "notification.show"
  ],
  "permission.request": [],
  "event.subscribe": [],
  "event.unsubscribe": [],
  "window.setTitle": [
    "window.control"
  ],
  "window.minimize": [
    "window.control"
  ],
  "window.maximize": [
    "window.control"
  ],
  "window.setSize": [
    "window.control"
  ],
  "window.close": [
    "window.control"
  ],
  "shell.openExternal": [
    "shell.open"
  ],
  "shell.openPath": [
    "shell.open"
  ],
  "shell.showInFolder": [
    "shell.open"
  ],
  "app.singleInstance.state": [],
  "app.singleInstance.focus": [],
  "tray.show": [
    "tray"
  ],
  "tray.hide": [
    "tray"
  ],
  "globalShortcut.register": [
    "globalShortcut"
  ],
  "globalShortcut.unregister": [
    "globalShortcut"
  ],
  "updater.check": [
    "updater"
  ],
  "updater.download": [
    "updater"
  ],
  "http.download": [
    "http.download"
  ],
  "sql.drivers": [
    "sql"
  ],
  "sql.open": [
    "sql"
  ],
  "sql.query": [
    "sql"
  ],
  "sql.execute": [
    "sql"
  ],
  "sql.close": [
    "sql"
  ],
  "path.dirs": [],
  "path.join": [],
  "path.normalize": [],
  "path.dirname": [],
  "path.basename": [],
  "path.extname": [],
  "path.isAbsolute": [],
  "store.get": [
    "store"
  ],
  "store.set": [
    "store"
  ],
  "store.delete": [
    "store"
  ],
  "store.clear": [
    "store"
  ],
  "store.keys": [
    "store"
  ],
  "os.getInfo": [
    "os.info"
  ],
  "log.write": [
    "log.write"
  ],
  "app.exit": [
    "window.control"
  ]
})KSMPERMISSIONS"
    );
    return value;
}

inline bool KSMGeneratedContract::ValidateArgs(
    const std::string& command,
    const nlohmann::json& args
) {
    const auto& schemas = ksm_contract_detail::ArgSchemas();
    if (!schemas.contains(command)) return false;
    return ksm_contract_detail::ValidateValue(args, schemas.at(command));
}
