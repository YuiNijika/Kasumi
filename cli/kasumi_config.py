from __future__ import annotations

import json
import re
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path
from typing import Any


SEMVER_PATTERN = re.compile(
    r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)"
    r"(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?"
    r"(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$"
)
WINDOWS_PACKAGE_PATTERN = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
ANDROID_PACKAGE_PATTERN = re.compile(
    r"^[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z][A-Za-z0-9_]*)+$"
)
PROTOCOL_COMMANDS_FILE = (
    Path(__file__).resolve().parent.parent / "protocol" / "commands.json"
)


class ConfigError(ValueError):
    pass


@dataclass(frozen=True)
class PlatformConfig:
    package_name: str
    version: str
    commands: tuple[str, ...]
    decorations: bool = True


@dataclass(frozen=True)
class KasumiConfig:
    windows: PlatformConfig
    android: PlatformConfig


def _expect_object(value: Any, location: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ConfigError(f"{location} must be an object.")
    return value


def _expect_string(value: Any, location: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ConfigError(f"{location} must be a non-empty string.")
    return value


def _reject_unknown_keys(
    value: dict[str, Any],
    allowed: set[str],
    location: str,
) -> None:
    unknown = sorted(set(value) - allowed)
    if unknown:
        names = ", ".join(unknown)
        raise ConfigError(f"{location} contains unknown fields: {names}.")


def _platform_config(
    value: Any,
    platform_name: str,
    platform: str,
    package_pattern: re.Pattern[str],
    allow_decorations: bool = False,
) -> PlatformConfig:
    location = platform
    data = _expect_object(value, location)
    allowed_fields = {"packageName", "version", "commands"}
    if allow_decorations:
        allowed_fields.add("decorations")
    _reject_unknown_keys(
        data,
        allowed_fields,
        location,
    )
    if "packageName" not in data:
        raise ConfigError(f"{location}.packageName is required.")
    if "version" not in data:
        raise ConfigError(f"{location}.version is required.")
    if "commands" not in data:
        raise ConfigError(f"{location}.commands is required.")

    package_name = _expect_string(
        data["packageName"],
        f"{location}.packageName",
    )
    if not package_pattern.fullmatch(package_name):
        raise ConfigError(
            f"{location}.packageName has an invalid format."
        )

    version = _expect_string(data["version"], f"{location}.version")
    if not SEMVER_PATTERN.fullmatch(version):
        raise ConfigError(
            f"{location}.version must be a semantic version."
        )

    decorations = True
    if allow_decorations and "decorations" in data:
        if not isinstance(data["decorations"], bool):
            raise ConfigError(
                f"{location}.decorations must be a boolean."
            )
        decorations = data["decorations"]

    commands = _parse_commands(data["commands"], platform_name, location)

    return PlatformConfig(
        package_name=package_name,
        version=version,
        commands=commands,
        decorations=decorations,
    )


def _parse_commands(
    value: Any,
    platform: str,
    location: str,
) -> tuple[str, ...]:
    if value == "all":
        return _available_commands(platform)
    if not isinstance(value, list) or not value:
        raise ConfigError(
            f"{location}.commands must be \"all\" or a non-empty array."
        )

    commands: list[str] = []
    for index, raw_command in enumerate(value):
        command = _expect_string(
            raw_command,
            f"{location}.commands[{index}]",
        )
        if command in commands:
            raise ConfigError(
                f"{location}.commands contains a duplicate: {command}."
            )
        commands.append(command)
    return tuple(commands)


@lru_cache(maxsize=2)
def _available_commands(platform: str) -> tuple[str, ...]:
    payload = json.loads(PROTOCOL_COMMANDS_FILE.read_text(encoding="utf-8"))
    commands = payload.get("commands")
    if not isinstance(commands, dict):
        raise RuntimeError("protocol/commands.json has no commands object.")

    result: list[str] = []
    for name, command in commands.items():
        platforms = command.get("platforms") if isinstance(command, dict) else None
        if isinstance(name, str) and isinstance(platforms, list) and platform in platforms:
            result.append(name)
    return tuple(result)


def load_config(path: Path) -> KasumiConfig:
    if not path.is_file():
        raise ConfigError(f"Configuration file not found: {path}")
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        raise ConfigError(
            f"Configuration file is not valid JSON: {path}: "
            f"line {error.lineno}, column {error.colno}."
        ) from error

    data = _expect_object(payload, "configuration")
    _reject_unknown_keys(data, {"$schema", "windows", "android"}, "configuration")
    if "windows" not in data:
        raise ConfigError("configuration.windows is required.")
    if "android" not in data:
        raise ConfigError("configuration.android is required.")

    return KasumiConfig(
        windows=_platform_config(
            data["windows"],
            "windows",
            "configuration.windows",
            WINDOWS_PACKAGE_PATTERN,
            True,
        ),
        android=_platform_config(
            data["android"],
            "android",
            "configuration.android",
            ANDROID_PACKAGE_PATTERN,
        ),
    )
