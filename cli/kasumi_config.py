from __future__ import annotations

import json
import re
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path
from typing import Any
from urllib.parse import urlsplit


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
PROTOCOL_PERMISSIONS_FILE = (
    Path(__file__).resolve().parent.parent / "protocol" / "permissions.schema.json"
)
PERMISSION_PATTERN = re.compile(
    r"^[a-z][A-Za-z0-9]*(?:\.[A-Za-z][A-Za-z0-9]*)*$"
)
ENTRY_URL_PATTERN = re.compile(
    r"^(https?)://([^/?#]+)([^?#]*)$"
)
SAFE_ENTRY_PATH_PATTERN = re.compile(
    r"^/(?:[A-Za-z0-9._-]+/)*[A-Za-z0-9._-]*$"
)
HTML_FILE_PATTERN = re.compile(
    r"^(?:[A-Za-z0-9._-]+/)*[A-Za-z0-9._-]+\.html$"
)
DEFAULT_WEBVIEW = {
    "productionEntry": "https://app.local/index.html",
    "developmentEntries": (
        "http://localhost:5173/index.html",
        "http://localhost:5173/",
        "http://localhost:5173",
    ),
    "supportedHtmlFiles": ("index.html",),
}


class ConfigError(ValueError):
    pass


@dataclass(frozen=True)
class WebViewConfig:
    production_entry: str
    development_entries: tuple[str, ...]
    supported_html_files: tuple[str, ...]


@dataclass(frozen=True)
class PlatformConfig:
    package_name: str
    version: str
    commands: tuple[str, ...]
    permissions: tuple[str, ...]
    decorations: bool = True
    shell_allowed_origins: tuple[str, ...] = ()
    http_allowed_origins: tuple[str, ...] = ()
    window: dict[str, Any] | None = None
    tray: dict[str, Any] | None = None
    icons: dict[str, str] | None = None
    resources: tuple[str, ...] = ()
    signing: dict[str, str] | None = None
    bundle: dict[str, Any] | None = None
    webview: WebViewConfig | None = None


@dataclass(frozen=True)
class KasumiConfig:
    windows: PlatformConfig
    android: PlatformConfig
    app_id: str | None = None
    icons: dict[str, str] | None = None
    resources: tuple[str, ...] = ()
    signing: dict[str, str] | None = None
    bundle: dict[str, Any] | None = None


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
    allow_webview: bool = False,
) -> PlatformConfig:
    location = platform
    data = _expect_object(value, location)
    allowed_fields = {
        "packageName",
        "version",
        "commands",
        "permissions",
        "shell",
        "http",
        "window",
        "tray",
        "icons",
        "resources",
        "signing",
        "bundle",
    }
    if allow_decorations:
        allowed_fields.add("decorations")
    if allow_webview:
        allowed_fields.add("webview")
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
    permissions = _parse_permissions(
        data.get("permissions"),
        f"{location}.permissions",
    )

    return PlatformConfig(
        package_name=package_name,
        version=version,
        commands=commands,
        permissions=permissions,
        decorations=decorations,
        shell_allowed_origins=_parse_origins(
            data.get("shell"),
            f"{location}.shell",
            {"http", "https"},
        ),
        http_allowed_origins=_parse_origins(
            data.get("http"),
            f"{location}.http",
            {"https"},
        ),
        window=_parse_window(
            data.get("window"),
            f"{location}.window",
            allow_decorations,
        ),
        tray=_parse_tray(data.get("tray"), f"{location}.tray"),
        icons=_parse_icons(data.get("icons"), f"{location}.icons"),
        resources=_parse_string_array(
            data.get("resources"),
            f"{location}.resources",
        ),
        signing=_parse_signing(
            data.get("signing"),
            f"{location}.signing",
        ),
        bundle=_parse_bundle(data.get("bundle"), f"{location}.bundle"),
        webview=_parse_webview(data.get("webview"), f"{location}.webview")
        if allow_webview
        else None,
    )


def _parse_webview(value: Any, location: str) -> WebViewConfig:
    if value is None:
        return WebViewConfig(
            production_entry=DEFAULT_WEBVIEW["productionEntry"],
            development_entries=DEFAULT_WEBVIEW["developmentEntries"],
            supported_html_files=DEFAULT_WEBVIEW["supportedHtmlFiles"],
        )
    data = _expect_object(value, location)
    _reject_unknown_keys(
        data,
        {"productionEntry", "developmentEntries", "supportedHtmlFiles"},
        location,
    )
    production_entry = _parse_production_entry(
        data.get("productionEntry", DEFAULT_WEBVIEW["productionEntry"]),
        f"{location}.productionEntry",
    )
    production_html_file = production_entry.split("://", 1)[1].partition("/")[2]
    supported_html_files = _parse_html_files(
        data.get("supportedHtmlFiles", DEFAULT_WEBVIEW["supportedHtmlFiles"]),
        f"{location}.supportedHtmlFiles",
    )
    if "index.html" not in supported_html_files:
        raise ConfigError(
            f"{location}.supportedHtmlFiles must include index.html "
            f"for frontend builds."
        )
    development_entries = _parse_development_entries(
        data.get("developmentEntries", DEFAULT_WEBVIEW["developmentEntries"]),
        f"{location}.developmentEntries",
        supported_html_files,
        production_html_file,
    )
    return WebViewConfig(
        production_entry=production_entry,
        development_entries=development_entries,
        supported_html_files=supported_html_files,
    )


def _parse_production_entry(value: Any, location: str) -> str:
    entry = _parse_entry_url(value, location, "https", "app.local", "")
    html_file = entry[1].lstrip("/")
    if not html_file.endswith(".html"):
        raise ConfigError(f"{location} must point to an HTML file.")
    return entry[0]


def _parse_development_entries(
    value: Any,
    location: str,
    supported_html_files: tuple[str, ...],
    production_html_file: str,
) -> tuple[str, ...]:
    if not isinstance(value, list) or not value:
        raise ConfigError(f"{location} must be a non-empty array.")
    entries: list[str] = []
    for index, raw_entry in enumerate(value):
        item_location = f"{location}[{index}]"
        entry = _parse_entry_url(
            raw_entry,
            item_location,
            "http",
            "localhost",
            ":5173",
        )
        html_file = entry[1].lstrip("/")
        if html_file and html_file not in supported_html_files:
            raise ConfigError(
                f"{item_location} is not in "
                f"configuration.android.webview.supportedHtmlFiles."
            )
        if entry[0] in entries:
            raise ConfigError(f"{location} contains a duplicate: {entry[0]}.")
        entries.append(entry[0])
    if not any(item.endswith(f"/{production_html_file}") for item in entries):
        raise ConfigError(
            f"{location} must include the production HTML file: "
            f"{production_html_file}."
        )
    return tuple(entries)


def _parse_entry_url(
    value: Any,
    location: str,
    scheme: str,
    authority: str,
    port: str,
) -> tuple[str, str]:
    text = _expect_string(value, location)
    if text.strip() != text:
        raise ConfigError(f"{location} is invalid.")
    match = ENTRY_URL_PATTERN.fullmatch(text)
    if match is None or match.group(1) != scheme:
        raise ConfigError(f"{location} is invalid.")
    actual_authority = match.group(2)
    path = match.group(3)
    if (
        actual_authority != f"{authority}{port}"
        or "?" in text
        or "#" in text
        or not _is_safe_entry_path(path)
    ):
        raise ConfigError(f"{location} is invalid.")
    return text, path


def _is_safe_entry_path(path: str) -> bool:
    if not path:
        return True
    return (
        SAFE_ENTRY_PATH_PATTERN.fullmatch(path) is not None
        and "%" not in path
        and "\\" not in path
        and "\u0000" not in path
    )


def _parse_html_files(value: Any, location: str) -> tuple[str, ...]:
    if not isinstance(value, list) or not value:
        raise ConfigError(f"{location} must be a non-empty array.")
    html_files: list[str] = []
    for index, raw_file in enumerate(value):
        item_location = f"{location}[{index}]"
        html_file = _expect_string(raw_file, item_location)
        if (
            HTML_FILE_PATTERN.fullmatch(html_file) is None
            or "\\" in html_file
            or "\u0000" in html_file
        ):
            raise ConfigError(f"{item_location} is invalid.")
        if html_file in html_files:
            raise ConfigError(f"{location} contains a duplicate: {html_file}.")
        html_files.append(html_file)
    return tuple(html_files)


def _parse_window(
    value: Any,
    location: str,
    allow_window_control: bool,
) -> dict[str, Any] | None:
    if value is None:
        return None
    data = _expect_object(value, location)
    _reject_unknown_keys(
        data,
        {
            "title",
            "decorations",
            "transparent",
            "alwaysOnTop",
            "center",
            "width",
            "height",
            "minWidth",
            "minHeight",
            "maxWidth",
            "maxHeight",
            "resizable",
            "visible",
        },
        location,
    )
    if not allow_window_control and data:
        raise ConfigError(f"{location} is not supported on this platform.")
    defaults: dict[str, Any] = {
        "title": "",
        "decorations": True,
        "transparent": False,
        "alwaysOnTop": False,
        "center": True,
        "width": 1080,
        "height": 720,
        "minWidth": 320,
        "minHeight": 240,
        "maxWidth": 0,
        "maxHeight": 0,
        "resizable": True,
        "visible": True,
    }
    result = {
        key: data[key] if key in data else default
        for key, default in defaults.items()
    }
    if not isinstance(result["title"], str):
        raise ConfigError(f"{location}.title must be a string.")
    for key in (
        "decorations",
        "transparent",
        "alwaysOnTop",
        "center",
        "resizable",
        "visible",
    ):
        if not isinstance(result[key], bool):
            raise ConfigError(f"{location}.{key} must be a boolean.")
    for key in (
        "width",
        "height",
        "minWidth",
        "minHeight",
        "maxWidth",
        "maxHeight",
    ):
        if (
            not isinstance(result[key], int)
            or isinstance(result[key], bool)
            or result[key] < 0
        ):
            raise ConfigError(
                f"{location}.{key} must be a non-negative integer."
            )
    return result


def _parse_tray(value: Any, location: str) -> dict[str, Any] | None:
    if value is None:
        return None
    data = _expect_object(value, location)
    _reject_unknown_keys(data, {"icon", "tooltip", "menu", "clickAction"}, location)
    result: dict[str, Any] = {}
    for key in ("icon", "tooltip", "clickAction"):
        if key in data:
            value_text = _expect_string(data[key], f"{location}.{key}")
            result[key] = value_text
    if "menu" in data:
        if not isinstance(data["menu"], list):
            raise ConfigError(f"{location}.menu must be an array.")
        menu: list[dict[str, str]] = []
        for index, item in enumerate(data["menu"]):
            item_location = f"{location}.menu[{index}]"
            item_data = _expect_object(item, item_location)
            _reject_unknown_keys(item_data, {"label", "action"}, item_location)
            if "label" not in item_data or "action" not in item_data:
                raise ConfigError(
                    f"{item_location} must contain label and action strings."
                )
            menu.append(
                {
                    "label": _expect_string(item_data["label"], f"{item_location}.label"),
                    "action": _expect_string(
                        item_data["action"], f"{item_location}.action"
                    ),
                }
            )
        result["menu"] = menu
    return result


def _parse_icons(value: Any, location: str) -> dict[str, str] | None:
    if value is None:
        return None
    data = _expect_object(value, location)
    _reject_unknown_keys(data, {"window", "tray", "installer"}, location)
    result: dict[str, str] = {}
    for key in ("window", "tray", "installer"):
        if key in data:
            result[key] = _expect_string(data[key], f"{location}.{key}")
    return result


def _parse_signing(value: Any, location: str) -> dict[str, str] | None:
    if value is None:
        return None
    data = _expect_object(value, location)
    _reject_unknown_keys(data, {"certificate", "password"}, location)
    result: dict[str, str] = {}
    for key in ("certificate", "password"):
        if key in data:
            result[key] = _expect_string(data[key], f"{location}.{key}")
    return result


def _parse_bundle(value: Any, location: str) -> dict[str, Any] | None:
    if value is None:
        return None
    data = _expect_object(value, location)
    _reject_unknown_keys(data, {"windowsInstaller"}, location)
    result: dict[str, Any] = {}
    if "windowsInstaller" in data:
        installer = data["windowsInstaller"]
        if installer not in ("nsis", "msi"):
            raise ConfigError(
                f"{location}.windowsInstaller must be \"nsis\" or \"msi\"."
            )
        result["windowsInstaller"] = installer
    return result


def _parse_string_array(value: Any, location: str) -> tuple[str, ...]:
    if value is None:
        return ()
    if not isinstance(value, list):
        raise ConfigError(f"{location} must be an array.")
    result: list[str] = []
    for index, item in enumerate(value):
        text = _expect_string(item, f"{location}[{index}]")
        result.append(text)
    return tuple(result)


def _parse_optional_string(value: Any, location: str) -> str | None:
    if value is None:
        return None
    return _expect_string(value, location)


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


def _parse_permissions(value: Any, location: str) -> tuple[str, ...]:
    available = _available_permissions()
    if value == "all":
        return available
    if value is None:
        return ()
    if not isinstance(value, list):
        raise ConfigError(
            f"{location} must be \"all\" or an array."
        )

    permissions: list[str] = []
    for index, raw_permission in enumerate(value):
        permission = _expect_string(
            raw_permission,
            f"{location}[{index}]",
        )
        if not PERMISSION_PATTERN.fullmatch(permission):
            raise ConfigError(f"{location}[{index}] has an invalid format.")
        if permission in permissions:
            raise ConfigError(
                f"{location} contains a duplicate: {permission}."
            )
        if permission not in available:
            raise ConfigError(
                f"{location} contains an unknown permission: {permission}."
            )
        permissions.append(permission)
    return tuple(permissions)


def _parse_origins(
    value: Any,
    location: str,
    protocols: set[str],
) -> tuple[str, ...]:
    if value is None:
        return ()
    data = _expect_object(value, location)
    _reject_unknown_keys(data, {"allowedOrigins"}, location)
    raw_origins = data.get("allowedOrigins", [])
    if not isinstance(raw_origins, list):
        raise ConfigError(f"{location}.allowedOrigins must be an array.")

    origins: list[str] = []
    for index, raw_origin in enumerate(raw_origins):
        origin_location = f"{location}.allowedOrigins[{index}]"
        origin = _expect_string(raw_origin, origin_location)
        if origin.strip() != origin:
            raise ConfigError(f"{origin_location} is invalid.")
        try:
            parsed = urlsplit(origin)
            port = parsed.port
        except ValueError as error:
            raise ConfigError(f"{origin_location} is invalid.") from error
        if (
            parsed.scheme not in protocols
            or not parsed.hostname
            or parsed.username is not None
            or parsed.password is not None
            or parsed.path not in ("", "/")
            or parsed.query
            or parsed.fragment
            or port == 0
        ):
            raise ConfigError(f"{origin_location} is invalid.")

        host = parsed.hostname.lower()
        host_text = f"[{host}]" if ":" in host else host
        default_port = 443 if parsed.scheme == "https" else 80
        port_text = f":{port}" if port is not None and port != default_port else ""
        normalized = f"{parsed.scheme}://{host_text}{port_text}"
        if normalized in origins:
            raise ConfigError(
                f"{location}.allowedOrigins contains a duplicate origin: "
                f"{normalized}."
            )
        origins.append(normalized)
    return tuple(origins)


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


@lru_cache(maxsize=1)
def _available_permissions() -> tuple[str, ...]:
    payload = json.loads(
        PROTOCOL_PERMISSIONS_FILE.read_text(encoding="utf-8")
    )
    properties = payload.get("properties")
    if not isinstance(properties, dict):
        raise RuntimeError(
            "protocol/permissions.schema.json has no properties object."
        )
    return tuple(
        name
        for name in properties
        if isinstance(name, str) and PERMISSION_PATTERN.fullmatch(name)
    )


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
    _reject_unknown_keys(
        data,
        {
            "$schema",
            "windows",
            "android",
            "extensions",
            "appId",
            "icons",
            "resources",
            "signing",
            "bundle",
        },
        "configuration",
    )
    if "windows" not in data:
        raise ConfigError("configuration.windows is required.")
    if "android" not in data:
        raise ConfigError("configuration.android is required.")

    return KasumiConfig(
        app_id=_parse_optional_string(data.get("appId"), "configuration.appId"),
        icons=_parse_icons(data.get("icons"), "configuration.icons"),
        resources=_parse_string_array(
            data.get("resources"), "configuration.resources"
        ),
        signing=_parse_signing(
            data.get("signing"), "configuration.signing"
        ),
        bundle=_parse_bundle(data.get("bundle"), "configuration.bundle"),
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
            allow_webview=True,
        ),
    )
