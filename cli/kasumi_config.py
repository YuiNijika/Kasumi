from __future__ import annotations

import json
import re
from dataclasses import dataclass, replace
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
PROTOCOL_PLUGINS_FILE = (
    Path(__file__).resolve().parent.parent / "protocol" / "plugins.json"
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
WINDOW_BACKGROUND_PATTERN = re.compile(r"^#(?:[0-9A-Fa-f]{6}|[0-9A-Fa-f]{8})$")
ERROR_PAGE_TEXT_PATTERN = re.compile(r'^[^"\\;\u0000-\u001F\u007F]+$')
EXTENSION_ID_PATTERN = re.compile(r"^[a-z0-9][a-z0-9-]*$")
APP_PLUGIN_VERSION_PATTERN = re.compile(r"^\d+\.\d+\.\d+$")
APP_PLUGIN_COMMAND_PATTERN = re.compile(
    r"^app\.[a-z][A-Za-z0-9]*(?:\.[A-Za-z][A-Za-z0-9]*)+$"
)
APP_PLUGIN_PERMISSION_PATTERN = re.compile(
    r"^[a-z][A-Za-z0-9]*(?:\.[A-Za-z][A-Za-z0-9]*)*$"
)
EXTENSION_PLATFORMS = ("windows", "android")
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
    framework_plugins: tuple[str, ...] = ()
    app_plugins: tuple[str, ...] = ()
    decorations: bool = True
    shell_allowed_origins: tuple[str, ...] = ()
    http_allowed_origins: tuple[str, ...] = ()
    window: dict[str, Any] | None = None
    error_page: dict[str, str] | None = None
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
    app_plugins: tuple[dict[str, Any], ...] = ()
    extensions: tuple[dict[str, Any], ...] = ()


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
    extension_commands: tuple[str, ...] = (),
    extension_permissions: tuple[str, ...] = (),
    app_plugin_definitions: tuple[dict[str, Any], ...] = (),
) -> PlatformConfig:
    location = platform
    data = _expect_object(value, location)
    allowed_fields = {
        "packageName",
        "version",
        "commands",
        "frameworkPlugins",
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
        allowed_fields.add("errorPage")
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
    if "commands" not in data and "frameworkPlugins" not in data:
        raise ConfigError(
            f"{location} must define commands or frameworkPlugins."
        )

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

    window = _parse_window(
        data.get("window"),
        f"{location}.window",
        allow_decorations,
    )
    decorations = True if window is None else window.get("decorations", True)
    if allow_decorations and "decorations" in data:
        if not isinstance(data["decorations"], bool):
            raise ConfigError(
                f"{location}.decorations must be a boolean."
            )
        decorations = data["decorations"]

    framework_plugin_mode = "frameworkPlugins" in data
    framework_plugins = _resolve_framework_plugins(
        data.get("frameworkPlugins"),
        platform_name,
        data.get("commands"),
        f"{location}.frameworkPlugins",
    )
    enabled_app_plugins = tuple(
        plugin["id"]
        for plugin in app_plugin_definitions
        if plugin.get("enabled", True)
        and platform_name in plugin["platforms"]
    )
    if framework_plugin_mode:
        base_commands = _plugin_commands_for_platform(
            framework_plugins,
            enabled_app_plugins,
            platform_name,
            app_plugin_definitions,
        )
        raw_commands = data.get("commands")
        if raw_commands is None or raw_commands == "all":
            commands = base_commands
        else:
            parsed_commands = _parse_commands(
                raw_commands,
                platform_name,
                location,
                extension_commands,
            )
            outside = [
                command for command in parsed_commands
                if command not in base_commands
            ]
            if outside:
                raise ConfigError(
                    f"{location}.commands contains a command outside enabled plugins: "
                    f"{outside[0]}."
                )
            commands = tuple(
                dict.fromkeys(
                    [
                        *parsed_commands,
                        "ping",
                        "capabilities.query",
                    ]
                )
            )
        default_permissions = _default_plugin_permissions(
            framework_plugins,
            enabled_app_plugins,
            app_plugin_definitions,
        )
    else:
        commands = _parse_commands(
            data["commands"],
            platform_name,
            location,
            extension_commands,
        )
        default_permissions = ()
    permissions = _parse_permissions(
        data.get("permissions")
        if "permissions" in data
        else default_permissions,
        f"{location}.permissions",
        extension_permissions,
    )

    return PlatformConfig(
        package_name=package_name,
        version=version,
        commands=commands,
        permissions=permissions,
        framework_plugins=framework_plugins,
        app_plugins=enabled_app_plugins,
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
        window=window,
        error_page=(
            _parse_error_page(
                data.get("errorPage"),
                f"{location}.errorPage",
            )
            if allow_decorations
            else None
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
            "backgroundColor",
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
        "backgroundColor": "",
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
    background_color = result["backgroundColor"]
    if not isinstance(background_color, str) or (
        background_color
        and not WINDOW_BACKGROUND_PATTERN.fullmatch(background_color)
    ):
        raise ConfigError(
            f"{location}.backgroundColor must be #RRGGBB or #AARRGGBB."
        )
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
    if result["maxWidth"] > 0 and result["maxWidth"] < result["minWidth"]:
        raise ConfigError(
            f"{location}.maxWidth must be greater than or equal to minWidth."
        )
    if result["maxHeight"] > 0 and result["maxHeight"] < result["minHeight"]:
        raise ConfigError(
            f"{location}.maxHeight must be greater than or equal to minHeight."
        )
    if result["maxWidth"] > 0 and result["width"] > result["maxWidth"]:
        raise ConfigError(
            f"{location}.width must be less than or equal to maxWidth."
        )
    if result["maxHeight"] > 0 and result["height"] > result["maxHeight"]:
        raise ConfigError(
            f"{location}.height must be less than or equal to maxHeight."
        )
    return result


def _parse_error_page(value: Any, location: str) -> dict[str, str] | None:
    if value is None:
        return None
    data = _expect_object(value, location)
    color_fields = (
        "backgroundColor",
        "cardColor",
        "borderColor",
        "textColor",
        "mutedTextColor",
        "accentColor",
        "errorColor",
    )
    text_fields = {
        "title": 80,
        "hint": 240,
    }
    _reject_unknown_keys(
        data,
        {*color_fields, *text_fields},
        location,
    )
    result: dict[str, str] = {}
    for key in color_fields:
        if key not in data:
            continue
        color = _expect_string(data[key], f"{location}.{key}")
        if not WINDOW_BACKGROUND_PATTERN.fullmatch(color):
            raise ConfigError(
                f"{location}.{key} must be #RRGGBB or #AARRGGBB."
            )
        result[key] = color
    for key, max_length in text_fields.items():
        if key not in data:
            continue
        text = _expect_string(data[key], f"{location}.{key}")
        if len(text) > max_length or not ERROR_PAGE_TEXT_PATTERN.fullmatch(text):
            raise ConfigError(
                f"{location}.{key} must be 1-{max_length} safe characters."
            )
        result[key] = text
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
    _reject_unknown_keys(
        data,
        {"window", "tray", "installer", "android"},
        location,
    )
    result: dict[str, str] = {}
    for key in ("window", "tray", "installer", "android"):
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
    extension_commands: tuple[str, ...] = (),
) -> tuple[str, ...]:
    if value == "all":
        commands = [*_available_commands(platform), *extension_commands]
        return tuple(dict.fromkeys(commands))
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


def _parse_permissions(
    value: Any,
    location: str,
    extension_permissions: tuple[str, ...] = (),
) -> tuple[str, ...]:
    available = tuple(
        dict.fromkeys([*_available_permissions(), *extension_permissions])
    )
    if value == "all":
        return available
    if value is None:
        return ()
    if not isinstance(value, (list, tuple)):
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


def _resolve_framework_plugins(
    value: Any,
    platform: str,
    commands: Any,
    location: str,
) -> tuple[str, ...]:
    definitions = _available_plugins()
    platform_ids = [
        plugin_id
        for plugin_id, definition in definitions.items()
        if platform in definition.get("platforms", [])
    ]
    if value is None:
        if commands == "all":
            return tuple(platform_ids)
        if not isinstance(commands, list):
            raise ConfigError(
                f"{location} requires frameworkPlugins when commands is absent."
            )
        selected = {
            plugin_id
            for plugin_id in platform_ids
            if plugin_id == "core"
            or any(
                command in commands
                for command in definitions[plugin_id].get("commands", [])
            )
        }
        return tuple(
            plugin_id for plugin_id in platform_ids if plugin_id in selected
        )
    if value == "all":
        return tuple(platform_ids)
    if not isinstance(value, list) or not value:
        raise ConfigError(f"{location} must be \"all\" or a non-empty array.")
    selected = {"core"}
    for index, plugin_id in enumerate(value):
        plugin_id = _expect_string(
            plugin_id,
            f"{location}[{index}]",
        )
        if plugin_id != "core" and plugin_id in selected:
            raise ConfigError(f"{location} contains a duplicate: {plugin_id}.")
        definition = definitions.get(plugin_id)
        if definition is None:
            raise ConfigError(
                f"{location} contains an unknown plugin: {plugin_id}."
            )
        if platform not in definition.get("platforms", []):
            raise ConfigError(
                f"{location} contains a plugin unsupported on {platform}: "
                f"{plugin_id}."
            )
        selected.add(plugin_id)
    return tuple(
        plugin_id for plugin_id in platform_ids if plugin_id in selected
    )


def _plugin_commands_for_platform(
    framework_plugins: tuple[str, ...],
    app_plugins: tuple[str, ...],
    platform: str,
    app_plugin_definitions: tuple[dict[str, Any], ...],
) -> tuple[str, ...]:
    definitions = _available_plugins()
    commands = ["ping", "capabilities.query"]
    for plugin_id in framework_plugins:
        for command in definitions[plugin_id].get("commands", []):
            if command not in commands:
                commands.append(command)
    for plugin in app_plugin_definitions:
        if plugin["id"] not in app_plugins:
            continue
        for command in plugin.get("commands", []):
            if (
                platform in command["platforms"]
                and command["name"] not in commands
            ):
                commands.append(command["name"])
    return tuple(commands)


def _default_plugin_permissions(
    framework_plugins: tuple[str, ...],
    app_plugins: tuple[str, ...],
    app_plugin_definitions: tuple[dict[str, Any], ...],
) -> tuple[str, ...]:
    definitions = _available_plugins()
    permissions: list[str] = []
    for plugin_id in framework_plugins:
        for permission in definitions[plugin_id].get("defaultPermissions", []):
            if permission not in permissions:
                permissions.append(permission)
    for plugin in app_plugin_definitions:
        if plugin["id"] not in app_plugins:
            continue
        for permission in plugin.get("defaultPermissions", []):
            if permission not in permissions:
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


@lru_cache(maxsize=1)
def _available_plugins() -> dict[str, dict[str, Any]]:
    payload = json.loads(PROTOCOL_PLUGINS_FILE.read_text(encoding="utf-8"))
    plugins = payload.get("plugins")
    if not isinstance(plugins, dict):
        raise RuntimeError("protocol/plugins.json has no plugins object.")
    return {
        name: definition
        for name, definition in plugins.items()
        if isinstance(name, str) and isinstance(definition, dict)
    }


@lru_cache(maxsize=1)
def _available_events() -> dict[str, dict[str, Any]]:
    payload = json.loads(PROTOCOL_COMMANDS_FILE.read_text(encoding="utf-8"))
    events = payload.get("events")
    if not isinstance(events, dict):
        raise RuntimeError("protocol/commands.json has no events object.")
    return {
        name: definition
        for name, definition in events.items()
        if isinstance(name, str) and isinstance(definition, dict)
    }


def _parse_app_plugins(
    value: Any,
    project_root: Path,
    location: str,
) -> tuple[
    tuple[dict[str, Any], ...],
    dict[str, tuple[str, ...]],
    tuple[str, ...],
]:
    empty_commands = {"windows": (), "android": ()}
    if value is None:
        return (), empty_commands, ()
    if not isinstance(value, list):
        raise ConfigError(f"{location} must be an array.")

    root = project_root.resolve()
    framework_plugins = _available_plugins()
    ids: set[str] = set()
    used_commands = set(_available_commands("windows")) | set(
        _available_commands("android")
    )
    used_events = set(_available_events())
    known_permissions = set(_available_permissions())
    app_plugins: list[dict[str, Any]] = []
    windows_commands: list[str] = []
    android_commands: list[str] = []
    custom_permissions: list[str] = []

    for index, entry in enumerate(value):
        item_location = f"{location}[{index}]"
        data = _expect_object(entry, item_location)
        _reject_unknown_keys(
            data,
            {"id", "manifest", "platforms", "enabled"},
            item_location,
        )
        app_id = _expect_string(
            data.get("id"),
            f"{item_location}.id",
        )
        if not EXTENSION_ID_PATTERN.fullmatch(app_id):
            raise ConfigError(f"{item_location}.id has an invalid format.")
        if app_id in ids:
            raise ConfigError(
                f"{item_location}.id is duplicated: {app_id}."
            )
        if app_id in framework_plugins:
            raise ConfigError(
                f"{item_location}.id conflicts with a framework plugin: "
                f"{app_id}."
            )
        enabled = data.get("enabled", True)
        if not isinstance(enabled, bool):
            raise ConfigError(f"{item_location}.enabled must be a boolean.")
        ids.add(app_id)
        platforms = _parse_extension_platforms(
            data.get("platforms"),
            f"{item_location}.platforms",
        )
        manifest_relative = _expect_string(
            data.get("manifest"),
            f"{item_location}.manifest",
        )
        manifest_path = (root / manifest_relative).resolve()
        try:
            manifest_path.relative_to(root)
        except ValueError as error:
            raise ConfigError(
                f"{item_location}.manifest must stay inside the project."
            ) from error
        try:
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        except FileNotFoundError as error:
            raise ConfigError(
                f"{item_location}.manifest was not found."
            ) from error
        except json.JSONDecodeError as error:
            raise ConfigError(
                f"{item_location}.manifest is not valid JSON."
            ) from error
        parsed = _parse_extension_manifest(
            manifest,
            manifest_relative,
            app_id,
            platforms,
            used_commands,
            used_events,
            known_permissions,
        )
        if enabled:
            for command in parsed["commands"]:
                if "windows" in command["platforms"]:
                    windows_commands.append(command["name"])
                if "android" in command["platforms"]:
                    android_commands.append(command["name"])
                for permission in command["permissions"]:
                    if (
                        permission not in known_permissions
                        and permission not in custom_permissions
                    ):
                        custom_permissions.append(permission)
        app_plugins.append(
            {
                "id": app_id,
                "manifest": manifest_relative,
                "platforms": platforms,
                "enabled": enabled,
                **parsed,
            }
        )

    return (
        tuple(app_plugins),
        {
            "windows": tuple(windows_commands),
            "android": tuple(android_commands),
        },
        tuple(custom_permissions),
    )


def _parse_extension_platforms(value: Any, location: str) -> tuple[str, ...]:
    if not isinstance(value, list) or not value:
        raise ConfigError(f"{location} must be a non-empty array.")
    platforms: list[str] = []
    for platform in value:
        if not isinstance(platform, str) or platform not in EXTENSION_PLATFORMS:
            raise ConfigError(f"{location} contains an invalid platform.")
        if platform in platforms:
            raise ConfigError(
                f"{location} contains a duplicate platform."
            )
        platforms.append(platform)
    return tuple(platforms)


def _parse_extension_manifest(
    value: Any,
    location: str,
    app_id: str,
    platforms: tuple[str, ...],
    used_commands: set[str],
    used_events: set[str],
    known_permissions: set[str],
) -> dict[str, Any]:
    data = _expect_object(value, location)
    _reject_unknown_keys(
        data,
        {
            "id",
            "name",
            "version",
            "description",
            "commands",
            "events",
            "defaultPermissions",
            "frameworkEvents",
        },
        location,
    )
    if data.get("id") != app_id:
        raise ConfigError(
            f"{location}.id must match configuration app plugin id."
        )
    name = (
        _expect_string(data.get("name"), f"{location}.name")
        if "name" in data
        else app_id
    )
    version = (
        _expect_string(data.get("version"), f"{location}.version")
        if "version" in data
        else "1.0.0"
    )
    if not APP_PLUGIN_VERSION_PATTERN.fullmatch(version):
        raise ConfigError(f"{location}.version must use semantic versioning.")
    description = (
        _expect_string(data.get("description"), f"{location}.description")
        if "description" in data
        else app_id
    )
    commands = _parse_extension_commands(
        data.get("commands"),
        f"{location}.commands",
        app_id,
        platforms,
        used_commands,
        known_permissions,
    )
    events = _parse_extension_events(
        data.get("events"),
        f"{location}.events",
        app_id,
        platforms,
        used_events,
    )
    command_permissions = {
        permission
        for command in commands
        for permission in command["permissions"]
    }
    default_permissions = _parse_app_plugin_permissions(
        data.get("defaultPermissions"),
        f"{location}.defaultPermissions",
        app_id,
        known_permissions,
        command_permissions,
    )
    framework_events = _parse_app_framework_events(
        data.get("frameworkEvents"),
        f"{location}.frameworkEvents",
        platforms,
    )
    return {
        "name": name,
        "version": version,
        "description": description,
        "commands": tuple(commands),
        "events": tuple(events),
        "defaultPermissions": default_permissions,
        "frameworkEvents": framework_events,
    }


def _parse_extension_commands(
    value: Any,
    location: str,
    app_id: str,
    platforms: tuple[str, ...],
    used_commands: set[str],
    known_permissions: set[str],
) -> list[dict[str, Any]]:
    if value is None:
        return []
    if not isinstance(value, list):
        raise ConfigError(f"{location} must be an array.")
    seen: set[str] = set()
    commands: list[dict[str, Any]] = []
    for index, entry in enumerate(value):
        item_location = f"{location}[{index}]"
        data = _expect_object(entry, item_location)
        _reject_unknown_keys(
            data,
            {"name", "platforms", "permissions", "args", "result"},
            item_location,
        )
        name = _expect_string(data.get("name"), f"{item_location}.name")
        if (
            not APP_PLUGIN_COMMAND_PATTERN.fullmatch(name)
            or not name.startswith(f"app.{app_id}.")
        ):
            raise ConfigError(
                f"{item_location}.name must use the app.{app_id}.* namespace."
            )
        if name in seen or name in used_commands:
            raise ConfigError(f"{item_location}.name conflicts: {name}.")
        seen.add(name)
        used_commands.add(name)
        declared = _parse_extension_platforms(
            data.get("platforms"),
            f"{item_location}.platforms",
        )
        if any(platform not in platforms for platform in declared):
            raise ConfigError(
                f"{item_location}.platforms exceeds the extension platform list."
            )
        permissions: list[str] = []
        raw_permissions = data.get("permissions")
        if raw_permissions is not None:
            if not isinstance(raw_permissions, list):
                raise ConfigError(
                    f"{item_location}.permissions must be an array."
                )
            for permission_index, raw_permission in enumerate(raw_permissions):
                permission = _expect_string(
                    raw_permission,
                    f"{item_location}.permissions[{permission_index}]",
                )
                if (
                    not APP_PLUGIN_PERMISSION_PATTERN.fullmatch(permission)
                    or (
                        permission not in known_permissions
                        and not _extension_permission_matches(
                            permission,
                            name,
                        )
                    )
                ):
                    raise ConfigError(
                        f"{item_location}.permissions contains an unknown permission."
                    )
                if permission not in permissions:
                    permissions.append(permission)
        commands.append(
            {
                "name": name,
                "platforms": declared,
                "permissions": tuple(permissions),
                "args": data.get("args") or {},
                "result": data.get("result") or {},
            }
        )
    return commands


def _parse_extension_events(
    value: Any,
    location: str,
    app_id: str,
    platforms: tuple[str, ...],
    used_events: set[str],
) -> list[dict[str, Any]]:
    if value is None:
        return []
    if not isinstance(value, list):
        raise ConfigError(f"{location} must be an array.")
    seen: set[str] = set()
    events: list[dict[str, Any]] = []
    for index, entry in enumerate(value):
        item_location = f"{location}[{index}]"
        data = _expect_object(entry, item_location)
        _reject_unknown_keys(
            data,
            {"name", "platforms", "payload"},
            item_location,
        )
        name = _expect_string(data.get("name"), f"{item_location}.name")
        if (
            not APP_PLUGIN_COMMAND_PATTERN.fullmatch(name)
            or not name.startswith(f"app.{app_id}.")
        ):
            raise ConfigError(
                f"{item_location}.name must use the app.{app_id}.* namespace."
            )
        if name in seen or name in used_events:
            raise ConfigError(f"{item_location}.name is duplicated: {name}.")
        seen.add(name)
        used_events.add(name)
        declared = _parse_extension_platforms(
            data.get("platforms"),
            f"{item_location}.platforms",
        )
        if any(platform not in platforms for platform in declared):
            raise ConfigError(
                f"{item_location}.platforms exceeds the extension platform list."
            )
        events.append(
            {
                "name": name,
                "platforms": declared,
                "payload": data.get("payload") or {},
            }
        )
    return events


def _parse_app_plugin_permissions(
    value: Any,
    location: str,
    app_id: str,
    known_permissions: set[str],
    command_permissions: set[str],
) -> tuple[str, ...]:
    if value is None:
        return ()
    if not isinstance(value, list):
        raise ConfigError(f"{location} must be an array.")
    permissions: list[str] = []
    for index, raw_permission in enumerate(value):
        permission = _expect_string(raw_permission, f"{location}[{index}]")
        if (
            not APP_PLUGIN_PERMISSION_PATTERN.fullmatch(permission)
            or (
                permission not in known_permissions
                and not permission.startswith(f"app.{app_id}")
            )
        ):
            raise ConfigError(f"{location} contains an unknown permission.")
        if permission not in command_permissions:
            raise ConfigError(
                f"{location} must belong to the plugin command permissions."
            )
        if permission not in permissions:
            permissions.append(permission)
    return tuple(permissions)


def _parse_app_framework_events(
    value: Any,
    location: str,
    platforms: tuple[str, ...],
) -> tuple[str, ...]:
    if value is None:
        return ()
    if not isinstance(value, list):
        raise ConfigError(f"{location} must be an array.")
    definitions = _available_events()
    events: list[str] = []
    for index, event in enumerate(value):
        event_name = _expect_string(event, f"{location}[{index}]")
        definition = definitions.get(event_name)
        if definition is None:
            raise ConfigError(
                f"{location} contains an unknown framework event."
            )
        event_platforms = definition.get("platforms", [])
        if not any(platform in platforms for platform in event_platforms):
            raise ConfigError(
                f"{location} event has no platform overlap with the app plugin."
            )
        if event_name not in events:
            events.append(event_name)
    return tuple(events)


def _extension_permission_matches(permission: str, command: str) -> bool:
    segments = command.split(".")
    if len(segments) < 2:
        return False
    namespace = ".".join(segments[:2])
    return permission == namespace or permission.startswith(f"{namespace}.")


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
            "appPlugins",
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
    if "appPlugins" in data and "extensions" in data:
        raise ConfigError(
            "configuration.appPlugins and configuration.extensions "
            "cannot be used together."
        )

    app_plugins, extension_commands, extension_permissions = _parse_app_plugins(
        data["appPlugins"] if "appPlugins" in data else data.get("extensions"),
        path.parent,
        (
            "configuration.appPlugins"
            if "appPlugins" in data
            else "configuration.extensions"
        ),
    )

    root_icons = _parse_icons(data.get("icons"), "configuration.icons")
    windows = _platform_config(
        data["windows"],
        "windows",
        "configuration.windows",
        WINDOWS_PACKAGE_PATTERN,
        True,
        extension_commands["windows"],
        extension_permissions,
        app_plugins,
    )
    android = _platform_config(
        data["android"],
        "android",
        "configuration.android",
        ANDROID_PACKAGE_PATTERN,
        allow_webview=True,
        extension_commands=extension_commands["android"],
        extension_permissions=extension_permissions,
        app_plugin_definitions=app_plugins,
    )
    windows = replace(
        windows,
        icons={**dict(root_icons or {}), **dict(windows.icons or {})} or None,
    )
    android = replace(
        android,
        icons={**dict(root_icons or {}), **dict(android.icons or {})} or None,
    )

    return KasumiConfig(
        app_id=_parse_optional_string(data.get("appId"), "configuration.appId"),
        icons=root_icons,
        resources=_parse_string_array(
            data.get("resources"), "configuration.resources"
        ),
        signing=_parse_signing(
            data.get("signing"), "configuration.signing"
        ),
        bundle=_parse_bundle(data.get("bundle"), "configuration.bundle"),
        windows=windows,
        android=android,
        app_plugins=app_plugins,
        extensions=app_plugins,
    )
