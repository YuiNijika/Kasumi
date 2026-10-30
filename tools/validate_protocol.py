from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
PROTOCOL = ROOT / "protocol"
ALLOWED_PLATFORMS = {"windows", "android"}
REQUIRED_COMMAND_FIELDS = {"args", "result", "permissions", "platforms", "since"}
REQUIRED_EVENT_FIELDS = {"payload", "platforms", "since"}
REQUIRED_ERROR_CODES = {
    "INVALID_ARGS",
    "UNKNOWN_COMMAND",
    "PERMISSION_DENIED",
    "NOT_SUPPORTED",
    "NOT_FOUND",
    "IO_ERROR",
    "CANCELLED",
    "TIMEOUT",
    "BUSY",
    "INTERNAL_ERROR",
}
SCHEMA_KEYS = {
    "type",
    "properties",
    "required",
    "additionalProperties",
    "items",
    "enum",
    "const",
    "minLength",
    "maxBytes",
}
NAME_PATTERN = re.compile(r"^[A-Za-z][A-Za-z0-9]*(?:\.[A-Za-z][A-Za-z0-9]*)*$")
VERSION_PATTERN = re.compile(r"^\d+\.\d+\.\d+$")


def load_json(path: Path) -> Any:
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


def require_object(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ValueError(f"{label} must be an object.")
    return value


def validate_string_list(
    value: Any,
    label: str,
    allowed: set[str] | None = None,
    allow_empty: bool = False,
) -> list[str]:
    if not isinstance(value, list) or (not value and not allow_empty):
        raise ValueError(f"{label} must be an array.")
    if any(not isinstance(item, str) or not item for item in value):
        raise ValueError(f"{label} must contain non-empty strings.")
    if len(value) != len(set(value)):
        raise ValueError(f"{label} must not contain duplicates.")
    if allowed is not None and not set(value) <= allowed:
        raise ValueError(f"{label} contains an unsupported value.")
    return value


def validate_value_schema(schema: Any, label: str) -> None:
    schema = require_object(schema, label)
    unknown = set(schema) - SCHEMA_KEYS
    if unknown:
        raise ValueError(f"{label} has unknown keys: {sorted(unknown)}")
    if "type" not in schema:
        raise ValueError(f"{label} must declare a type.")

    schema_type = schema["type"]
    types = schema_type if isinstance(schema_type, list) else [schema_type]
    if (
        not types
        or len(types) != len(set(types))
        or not set(types)
        <= {"object", "array", "string", "number", "integer", "boolean", "null"}
    ):
        raise ValueError(f"{label} has an invalid type.")

    if "properties" in schema:
        properties = require_object(schema["properties"], f"{label}.properties")
        for name, child in properties.items():
            if not isinstance(name, str) or not name:
                raise ValueError(f"{label}.properties has an invalid key.")
            validate_value_schema(child, f"{label}.properties.{name}")

    if "required" in schema:
        required = validate_string_list(
            schema["required"],
            f"{label}.required",
            allow_empty=True,
        )
        properties = schema.get("properties", {})
        if not isinstance(properties, dict) or not set(required) <= set(properties):
            raise ValueError(f"{label}.required references an unknown property.")

    if "additionalProperties" in schema and not isinstance(
        schema["additionalProperties"], bool
    ):
        raise ValueError(f"{label}.additionalProperties must be a boolean.")

    if "items" in schema:
        if "array" not in types:
            raise ValueError(f"{label}.items requires an array type.")
        validate_value_schema(schema["items"], f"{label}.items")

    if "enum" in schema:
        enum = schema["enum"]
        if not isinstance(enum, list) or not enum:
            raise ValueError(f"{label}.enum must be a non-empty array.")

    if "minLength" in schema and (
        not isinstance(schema["minLength"], int) or schema["minLength"] < 0
    ):
        raise ValueError(f"{label}.minLength must be a non-negative integer.")

    if "maxBytes" in schema and (
        not isinstance(schema["maxBytes"], int) or schema["maxBytes"] < 1
    ):
        raise ValueError(f"{label}.maxBytes must be a positive integer.")


def validate_object_schema(schema: Any, label: str) -> None:
    validate_value_schema(schema, label)
    if schema.get("type") != "object":
        raise ValueError(f"{label} must describe an object.")
    if schema.get("additionalProperties") is not False:
        raise ValueError(f"{label} must reject additional properties.")
    if not isinstance(schema.get("properties"), dict):
        raise ValueError(f"{label} must declare properties.")
    if not isinstance(schema.get("required"), list):
        raise ValueError(f"{label} must declare required fields.")


def validate_contract(contract: Any, permissions: Any, errors: Any) -> None:
    contract = require_object(contract, "commands.json")
    if set(contract) != {"commands", "events"}:
        raise ValueError("commands.json must contain only commands and events.")
    commands = require_object(contract["commands"], "commands")
    events = require_object(contract["events"], "events")
    if not commands or not events:
        raise ValueError("commands.json must define commands and events.")

    permission_schema = require_object(permissions, "permissions.schema.json")
    permission_properties = require_object(
        permission_schema.get("properties"),
        "permissions.schema.json.properties",
    )
    if permission_schema.get("additionalProperties") is not False:
        raise ValueError("permissions.schema.json must reject unknown permissions.")
    for name, definition in permission_properties.items():
        definition = require_object(
            definition,
            f"permissions.schema.json.properties.{name}",
        )
        if not isinstance(definition.get("description"), str):
            raise ValueError(f"Permission {name} must declare a description.")

    for name, definition in commands.items():
        if not isinstance(name, str) or not NAME_PATTERN.fullmatch(name):
            raise ValueError(f"Invalid command name: {name}")
        if name.startswith("KSM"):
            raise ValueError(f"Command names must not use the KSM prefix: {name}")
        definition = require_object(definition, f"commands.{name}")
        if set(definition) != REQUIRED_COMMAND_FIELDS:
            raise ValueError(
                f"{name} must declare exactly {sorted(REQUIRED_COMMAND_FIELDS)}"
            )
        validate_object_schema(definition["args"], f"{name}.args")
        validate_value_schema(definition["result"], f"{name}.result")
        result_types = definition["result"].get("type")
        result_types = result_types if isinstance(result_types, list) else [result_types]
        if (
            "object" in result_types
            and definition["result"].get("additionalProperties") is not False
        ):
            raise ValueError(f"{name}.result must reject additional properties.")
        validate_string_list(
            definition["permissions"],
            f"{name}.permissions",
            set(permission_properties),
            allow_empty=True,
        )
        validate_string_list(
            definition["platforms"],
            f"{name}.platforms",
            ALLOWED_PLATFORMS,
        )
        if not isinstance(definition["since"], str) or not VERSION_PATTERN.fullmatch(
            definition["since"]
        ):
            raise ValueError(f"{name}.since must use semantic versioning.")

    for name, definition in events.items():
        if not isinstance(name, str) or not NAME_PATTERN.fullmatch(name):
            raise ValueError(f"Invalid event name: {name}")
        if name.startswith("KSM"):
            raise ValueError(f"Event names must not use the KSM prefix: {name}")
        definition = require_object(definition, f"events.{name}")
        if set(definition) != REQUIRED_EVENT_FIELDS:
            raise ValueError(
                f"{name} must declare exactly {sorted(REQUIRED_EVENT_FIELDS)}"
            )
        validate_object_schema(definition["payload"], f"{name}.payload")
        validate_string_list(
            definition["platforms"],
            f"{name}.platforms",
            ALLOWED_PLATFORMS,
        )
        if not isinstance(definition["since"], str) or not VERSION_PATTERN.fullmatch(
            definition["since"]
        ):
            raise ValueError(f"{name}.since must use semantic versioning.")

    errors = require_object(errors, "errors.json")
    if set(errors) != REQUIRED_ERROR_CODES:
        raise ValueError("errors.json must contain the complete standard error set.")
    if any(not isinstance(message, str) or not message for message in errors.values()):
        raise ValueError("errors.json messages must be non-empty strings.")


def validate() -> None:
    validate_contract(
        load_json(PROTOCOL / "commands.json"),
        load_json(PROTOCOL / "permissions.schema.json"),
        load_json(PROTOCOL / "errors.json"),
    )


if __name__ == "__main__":
    validate()
    print("protocol valid")
