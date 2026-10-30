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
  "window.close": {
    "type": "object",
    "properties": {},
    "required": [],
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
    static bool ValidateArgs(const std::string& command, const nlohmann::json& args);
};

inline const std::vector<std::string>& KSMGeneratedContract::Commands() {
    static const std::vector<std::string> values = {
        "ping", "capabilities.query", "app.getInfo", "fs.readText", "fs.writeText", "fs.exists", "fs.remove", "fs.readDir", "fs.mkdir", "fs.stat", "dialog.message", "dialog.confirm", "dialog.openFile", "dialog.saveFile", "clipboard.readText", "clipboard.writeText", "notification.show", "permission.request", "event.subscribe", "event.unsubscribe", "window.setTitle", "window.minimize", "window.maximize", "window.close", "app.exit"
    };
    return values;
}

inline const std::vector<std::string>& KSMGeneratedContract::WindowsCommands() {
    static const std::vector<std::string> values = {
        "ping", "capabilities.query", "app.getInfo", "fs.readText", "fs.writeText", "fs.exists", "fs.remove", "fs.readDir", "fs.mkdir", "fs.stat", "dialog.message", "dialog.confirm", "dialog.openFile", "dialog.saveFile", "clipboard.readText", "clipboard.writeText", "notification.show", "permission.request", "event.subscribe", "event.unsubscribe", "window.setTitle", "window.minimize", "window.maximize", "window.close", "app.exit"
    };
    return values;
}

inline const std::vector<std::string>& KSMGeneratedContract::AndroidCommands() {
    static const std::vector<std::string> values = {
        "ping", "capabilities.query", "app.getInfo", "fs.readText", "fs.writeText", "fs.exists", "fs.remove", "fs.readDir", "fs.mkdir", "fs.stat", "dialog.message", "dialog.confirm", "dialog.openFile", "dialog.saveFile", "clipboard.readText", "clipboard.writeText", "notification.show", "permission.request", "event.subscribe", "event.unsubscribe", "window.close", "app.exit"
    };
    return values;
}

inline const std::vector<std::string>& KSMGeneratedContract::Events() {
    static const std::vector<std::string> values = {
        "webview.reloaded", "window.resized"
    };
    return values;
}

inline const std::vector<std::string>& KSMGeneratedContract::Permissions() {
    static const std::vector<std::string> values = {
        "fs.read", "fs.write", "fs.remove", "dialog", "clipboard.read", "clipboard.write", "notification.show", "window.control"
    };
    return values;
}

inline bool KSMGeneratedContract::ValidateArgs(
    const std::string& command,
    const nlohmann::json& args
) {
    const auto& schemas = ksm_contract_detail::ArgSchemas();
    if (!schemas.contains(command)) return false;
    return ksm_contract_detail::ValidateValue(args, schemas.at(command));
}
