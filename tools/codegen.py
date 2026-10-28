from __future__ import annotations

import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
PROTOCOL = ROOT / "protocol" / "commands.json"
PERMISSIONS = ROOT / "protocol" / "permissions.schema.json"
ERRORS = ROOT / "protocol" / "errors.json"
FRONTEND = ROOT / "frontend" / "src" / "generated"
WINDOWS = ROOT / "windows" / "src" / "generated"
ANDROID = (
    ROOT
    / "android"
    / "app"
    / "src"
    / "main"
    / "java"
    / "dev"
    / "kasumi"
    / "app"
    / "generated"
)


def load_json(path: Path) -> Any:
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


def json_text(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, indent=2)


def js_array(values: list[str], indent: int = 2) -> str:
    if not values:
        return "[]"
    padding = " " * indent
    return "[\n" + ",\n".join(f"{padding}{json.dumps(value)}" for value in values) + "\n" + " " * (indent - 2) + "]"


def kotlin_list(values: list[str]) -> str:
    if not values:
        return "emptyList()"
    return "listOf(\n" + ",\n".join(f'        "{value}"' for value in values) + "\n    )"


def commands_for(contract: dict[str, Any], platform: str) -> list[str]:
    return [
        name
        for name, definition in contract["commands"].items()
        if platform in definition["platforms"]
    ]


def permissions_for(permission_schema: dict[str, Any]) -> list[str]:
    return list(permission_schema["properties"].keys())


def arg_schemas(contract: dict[str, Any]) -> dict[str, Any]:
    return {
        name: definition["args"]
        for name, definition in contract["commands"].items()
    }


def generate_frontend(
    contract: dict[str, Any],
    permission_schema: dict[str, Any],
    errors: list[str],
) -> None:
    FRONTEND.mkdir(parents=True, exist_ok=True)
    commands = list(contract["commands"].keys())
    windows_commands = commands_for(contract, "windows")
    android_commands = commands_for(contract, "android")
    events = list(contract["events"].keys())
    permissions = permissions_for(permission_schema)
    schemas = json_text(arg_schemas(contract))
    content = f"""export const KSM_COMMANDS = {js_array(commands)} as const;
export const KSM_WINDOWS_COMMANDS = {js_array(windows_commands)} as const;
export const KSM_ANDROID_COMMANDS = {js_array(android_commands)} as const;
export const KSM_EVENTS = {js_array(events)} as const;
export const KSM_PERMISSIONS = {js_array(permissions)} as const;
export const KSM_ERROR_CODES = {js_array(errors)} as const;

export type KSMCommandName = (typeof KSM_COMMANDS)[number];
export type KSMEventName = (typeof KSM_EVENTS)[number];
export type KSMPermission = (typeof KSM_PERMISSIONS)[number];

interface KSMValueSchema {{
  type?: string | readonly string[];
  properties?: Readonly<Record<string, KSMValueSchema>>;
  required?: readonly string[];
  additionalProperties?: boolean;
  items?: KSMValueSchema;
  enum?: readonly unknown[];
  const?: unknown;
  minLength?: number;
  maxBytes?: number;
}}

const KSM_ARG_SCHEMAS: Readonly<Record<string, KSMValueSchema>> = {schemas};

function isRecord(value: unknown): value is Record<string, unknown> {{
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}}

function matchesType(value: unknown, type: string): boolean {{
  switch (type) {{
    case 'object':
      return isRecord(value);
    case 'array':
      return Array.isArray(value);
    case 'string':
      return typeof value === 'string';
    case 'number':
      return typeof value === 'number' && Number.isFinite(value);
    case 'integer':
      return typeof value === 'number' && Number.isInteger(value);
    case 'boolean':
      return typeof value === 'boolean';
    case 'null':
      return value === null;
    default:
      return false;
  }}
}}

function sameValue(left: unknown, right: unknown): boolean {{
  return Object.is(left, right) || JSON.stringify(left) === JSON.stringify(right);
}}

function validateValue(value: unknown, schema: KSMValueSchema): boolean {{
  const rawTypes = schema.type;
  const types = typeof rawTypes === 'string'
    ? [rawTypes]
    : Array.isArray(rawTypes)
      ? rawTypes
      : [];
  if (types.length === 0 || !types.some((type) => matchesType(value, type))) {{
    return false;
  }}

  if (schema.enum !== undefined && !schema.enum.some((candidate) => sameValue(candidate, value))) {{
    return false;
  }}
  if (schema.const !== undefined && !sameValue(schema.const, value)) {{
    return false;
  }}

  if (isRecord(value)) {{
    const properties = schema.properties ?? {{}};
    for (const field of schema.required ?? []) {{
      if (!Object.prototype.hasOwnProperty.call(value, field)) {{
        return false;
      }}
    }}
    if (schema.additionalProperties === false) {{
      for (const field of Object.keys(value)) {{
        if (!Object.prototype.hasOwnProperty.call(properties, field)) {{
          return false;
        }}
      }}
    }}
    for (const [field, propertySchema] of Object.entries(properties)) {{
      if (
        Object.prototype.hasOwnProperty.call(value, field) &&
        !validateValue(value[field], propertySchema)
      ) {{
        return false;
      }}
    }}
  }}

  if (Array.isArray(value) && schema.items) {{
    if (!value.every((item) => validateValue(item, schema.items as KSMValueSchema))) {{
      return false;
    }}
  }}

  if (typeof value === 'string') {{
    if (schema.minLength !== undefined && value.length < schema.minLength) {{
      return false;
    }}
    if (
      schema.maxBytes !== undefined &&
      new TextEncoder().encode(value).byteLength > schema.maxBytes
    ) {{
      return false;
    }}
  }}

  return true;
}}

export function validateKSMArgs(command: string, args: unknown): boolean {{
  const schema = KSM_ARG_SCHEMAS[command];
  return schema !== undefined && validateValue(args, schema);
}}
"""
    (FRONTEND / "contract.ts").write_text(content, encoding="utf-8")


def generate_windows(
    contract: dict[str, Any],
    permission_schema: dict[str, Any],
    errors: list[str],
) -> None:
    WINDOWS.mkdir(parents=True, exist_ok=True)
    commands = list(contract["commands"].keys())
    windows_commands = commands_for(contract, "windows")
    android_commands = commands_for(contract, "android")
    events = list(contract["events"].keys())
    permissions = permissions_for(permission_schema)
    schema_json = json_text(arg_schemas(contract)).replace(
        ")KSMSCHEMA",
        ")KSMSCHEMA_ESCAPED",
    )
    content = f"""#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace ksm_contract_detail {{

using Json = nlohmann::json;

inline const Json& ArgSchemas() {{
    static const Json schemas = Json::parse(R"KSMSCHEMA({schema_json})KSMSCHEMA");
    return schemas;
}}

inline bool MatchesType(const Json& value, const std::string& type) {{
    if (type == "object") return value.is_object();
    if (type == "array") return value.is_array();
    if (type == "string") return value.is_string();
    if (type == "number") return value.is_number();
    if (type == "integer") return value.is_number_integer() || value.is_number_unsigned();
    if (type == "boolean") return value.is_boolean();
    if (type == "null") return value.is_null();
    return false;
}}

inline bool SameValue(const Json& left, const Json& right) {{
    return left == right;
}}

inline bool ValidateValue(const Json& value, const Json& schema) {{
    if (!schema.contains("type")) return false;
    const Json& type = schema.at("type");
    bool matches = false;
    if (type.is_string()) {{
        matches = MatchesType(value, type.get<std::string>());
    }} else if (type.is_array()) {{
        for (const auto& item : type) {{
            if (item.is_string() && MatchesType(value, item.get<std::string>())) {{
                matches = true;
                break;
            }}
        }}
    }}
    if (!matches) return false;

    if (schema.contains("enum")) {{
        bool found = false;
        for (const auto& candidate : schema.at("enum")) {{
            if (SameValue(candidate, value)) {{
                found = true;
                break;
            }}
        }}
        if (!found) return false;
    }}
    if (schema.contains("const") && !SameValue(schema.at("const"), value)) {{
        return false;
    }}

    if (value.is_object()) {{
        const Json properties = schema.value("properties", Json::object());
        if (schema.contains("required")) {{
            for (const auto& field : schema.at("required")) {{
                if (!value.contains(field.get<std::string>())) return false;
            }}
        }}
        if (schema.value("additionalProperties", true) == false) {{
            for (const auto& [field, unused] : value.items()) {{
                (void)unused;
                if (!properties.contains(field)) return false;
            }}
        }}
        for (const auto& [field, property_schema] : properties.items()) {{
            if (value.contains(field) && !ValidateValue(value.at(field), property_schema)) {{
                return false;
            }}
        }}
    }}

    if (value.is_array() && schema.contains("items")) {{
        for (const auto& item : value) {{
            if (!ValidateValue(item, schema.at("items"))) return false;
        }}
    }}

    if (value.is_string()) {{
        const std::string text = value.get<std::string>();
        if (schema.contains("minLength") && text.size() < schema.at("minLength").get<std::size_t>()) {{
            return false;
        }}
        if (schema.contains("maxBytes") && text.size() > schema.at("maxBytes").get<std::size_t>()) {{
            return false;
        }}
    }}
    return true;
}}

}}  // namespace ksm_contract_detail

class KSMGeneratedContract {{
public:
    static const std::vector<std::string>& Commands();
    static const std::vector<std::string>& WindowsCommands();
    static const std::vector<std::string>& AndroidCommands();
    static const std::vector<std::string>& Events();
    static const std::vector<std::string>& Permissions();
    static bool ValidateArgs(const std::string& command, const nlohmann::json& args);
}};

inline const std::vector<std::string>& KSMGeneratedContract::Commands() {{
    static const std::vector<std::string> values = {{
        {", ".join(json.dumps(value) for value in commands)}
    }};
    return values;
}}

inline const std::vector<std::string>& KSMGeneratedContract::WindowsCommands() {{
    static const std::vector<std::string> values = {{
        {", ".join(json.dumps(value) for value in windows_commands)}
    }};
    return values;
}}

inline const std::vector<std::string>& KSMGeneratedContract::AndroidCommands() {{
    static const std::vector<std::string> values = {{
        {", ".join(json.dumps(value) for value in android_commands)}
    }};
    return values;
}}

inline const std::vector<std::string>& KSMGeneratedContract::Events() {{
    static const std::vector<std::string> values = {{
        {", ".join(json.dumps(value) for value in events)}
    }};
    return values;
}}

inline const std::vector<std::string>& KSMGeneratedContract::Permissions() {{
    static const std::vector<std::string> values = {{
        {", ".join(json.dumps(value) for value in permissions)}
    }};
    return values;
}}

inline bool KSMGeneratedContract::ValidateArgs(
    const std::string& command,
    const nlohmann::json& args
) {{
    const auto& schemas = ksm_contract_detail::ArgSchemas();
    if (!schemas.contains(command)) return false;
    return ksm_contract_detail::ValidateValue(args, schemas.at(command));
}}
"""
    (WINDOWS / "contract.h").write_text(content, encoding="utf-8")
    error_content = "#pragma once\n\nnamespace KSMErrorCode {\n"
    error_content += "".join(
        f'inline constexpr char {error}[] = "{error}";\n' for error in errors
    )
    error_content += "}\n"
    (WINDOWS / "errors.h").write_text(error_content, encoding="utf-8")


def generate_android(
    contract: dict[str, Any],
    permission_schema: dict[str, Any],
    errors: list[str],
) -> None:
    ANDROID.mkdir(parents=True, exist_ok=True)
    commands = list(contract["commands"].keys())
    windows_commands = commands_for(contract, "windows")
    android_commands = commands_for(contract, "android")
    events = list(contract["events"].keys())
    permissions = permissions_for(permission_schema)
    schema_json = json_text(arg_schemas(contract))
    content = f"""package dev.kasumi.app.generated

import org.json.JSONArray
import org.json.JSONObject

object KSMGeneratedContract {{
    val commands: List<String> = {kotlin_list(commands)}

    val windowsCommands: List<String> = {kotlin_list(windows_commands)}

    val androidCommands: List<String> = {kotlin_list(android_commands)}

    val events: List<String> = {kotlin_list(events)}

    val permissions: List<String> = {kotlin_list(permissions)}

    private const val ARG_SCHEMAS_JSON = \"\"\"{schema_json}\"\"\"
    private val argSchemas = JSONObject(ARG_SCHEMAS_JSON)

    fun isKnownCommand(command: String): Boolean = argSchemas.has(command)

    fun validateArgs(command: String, args: JSONObject): Boolean {{
        val schema = argSchemas.optJSONObject(command) ?: return false
        return validateValue(args, schema)
    }}

    private fun validateValue(value: Any?, schema: JSONObject): Boolean {{
        val rawType = schema.opt("type")
        val types: List<String> = when (rawType) {{
            is String -> listOf(rawType)
            is JSONArray -> (0 until rawType.length()).mapNotNull {{ rawType.optString(it, "") }}
            else -> emptyList()
        }}
        if (types.isEmpty() || types.none {{ matchesType(value, it) }}) {{
            return false
        }}

        if (schema.has("enum")) {{
            val candidates = schema.optJSONArray("enum") ?: return false
            var found = false
            for (index in 0 until candidates.length()) {{
                if (sameValue(candidates.opt(index), value)) {{
                    found = true
                    break
                }}
            }}
            if (!found) return false
        }}
        if (schema.has("const") && !sameValue(schema.opt("const"), value)) {{
            return false
        }}

        if (value is JSONObject) {{
            val properties = schema.optJSONObject("properties") ?: JSONObject()
            val required = schema.optJSONArray("required")
            if (required != null) {{
                for (index in 0 until required.length()) {{
                    if (!value.has(required.optString(index))) return false
                }}
            }}
            if (schema.optBoolean("additionalProperties", true).not()) {{
                val keys = value.keys()
                while (keys.hasNext()) {{
                    if (!properties.has(keys.next())) return false
                }}
            }}
            val propertyKeys = properties.keys()
            while (propertyKeys.hasNext()) {{
                val field = propertyKeys.next()
                val propertySchema = properties.optJSONObject(field) ?: continue
                if (value.has(field) && !validateValue(value.opt(field), propertySchema)) {{
                    return false
                }}
            }}
        }}

        if (value is JSONArray && schema.has("items")) {{
            val itemSchema = schema.optJSONObject("items") ?: return false
            for (index in 0 until value.length()) {{
                if (!validateValue(value.opt(index), itemSchema)) return false
            }}
        }}

        if (value is String) {{
            if (schema.has("minLength") && value.length < schema.optInt("minLength")) {{
                return false
            }}
            if (
                schema.has("maxBytes") &&
                value.toByteArray(Charsets.UTF_8).size > schema.optInt("maxBytes")
            ) {{
                return false
            }}
        }}
        return true
    }}

    private fun matchesType(value: Any?, type: String): Boolean {{
        return when (type) {{
            "object" -> value is JSONObject
            "array" -> value is JSONArray
            "string" -> value is String
            "number" -> value is Number
            "integer" -> value is Number && value.toDouble() % 1.0 == 0.0
            "boolean" -> value is Boolean
            "null" -> value == null || value === JSONObject.NULL
            else -> false
        }}
    }}

    private fun sameValue(left: Any?, right: Any?): Boolean {{
        return left == right || left?.toString() == right?.toString()
    }}
}}
"""
    (ANDROID / "KSMGeneratedContract.kt").write_text(content, encoding="utf-8")
    error_content = "package dev.kasumi.app.generated\n\nobject KSMErrorCode {\n"
    error_content += "".join(
        f'    const val {error} = "{error}"\n' for error in errors
    )
    error_content += "}\n"
    (ANDROID / "KSMErrorCode.kt").write_text(error_content, encoding="utf-8")


def main() -> None:
    contract = load_json(PROTOCOL)
    permission_schema = load_json(PERMISSIONS)
    errors = list(load_json(ERRORS).keys())
    generate_frontend(contract, permission_schema, errors)
    generate_windows(contract, permission_schema, errors)
    generate_android(contract, permission_schema, errors)


if __name__ == "__main__":
    main()
