from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
SEMVER_PATTERN = re.compile(
    r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)"
    r"(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?"
    r"(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$"
)
VERSION_SENSITIVE_SOURCES = (
    "windows/CMakeLists.txt",
    "windows/examples/win32-host/CMakeLists.txt",
    "windows/src/bridge/ksm_router.cpp",
    "windows/src/platform/ksm_webview2_host.h",
    "windows/src/platform/ksm_windows_host_services.h",
    "windows/tests/contract_runner.cpp",
    "android/build.gradle.kts",
    "android/app/build.gradle.kts",
    "android/app/src/androidTest/java/dev/kasumi/app/bridge/"
    "BridgeInstrumentedTest.kt",
    "android/kasumi/build.gradle.kts",
    "android/kasumi/src/main/java/dev/kasumi/app/"
    "KSMAndroidHostServices.kt",
    "android/kasumi/src/main/java/dev/kasumi/app/bridge/"
    "KSMHostServices.kt",
    "android/kasumi/src/main/java/dev/kasumi/app/bridge/KSMRouter.kt",
    "examples/android-host/build.gradle.kts",
    "examples/android-host/app/build.gradle.kts",
    "packages/create-kasumi/src/cli.mjs",
    "tests/contract/adapters/mock-host.ts",
)


class VersionMetadataError(ValueError):
    pass


def validate_version(version: str) -> str:
    if SEMVER_PATTERN.fullmatch(version) is None:
        raise VersionMetadataError(f"Invalid semantic version: {version}")
    return version


def read_version(root: Path = ROOT) -> str:
    path = root / "version.json"
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise VersionMetadataError(
            f"Unable to read version metadata from {path}: {error}"
        ) from error
    if not isinstance(payload, dict):
        raise VersionMetadataError(f"Version metadata must be an object: {path}")
    unknown_fields = set(payload) - {"$schema", "version"}
    if unknown_fields:
        fields = ", ".join(sorted(unknown_fields))
        raise VersionMetadataError(f"Unknown version metadata fields: {fields}")
    version = payload.get("version")
    if not isinstance(version, str):
        raise VersionMetadataError(f"Version metadata has no string version: {path}")
    return validate_version(version)


def derived_versions(root: Path = ROOT) -> dict[str, str]:
    return {
        "version.json": read_version(root),
        "package.json": _json_version(root / "package.json"),
        "packages/kasumi-bridge/package.json": _json_version(
            root / "packages" / "kasumi-bridge" / "package.json"
        ),
        "packages/kasumi-cli/package.json": _json_version(
            root / "packages" / "kasumi-cli" / "package.json"
        ),
        "packages/create-kasumi/package.json": _json_version(
            root / "packages" / "create-kasumi" / "package.json"
        ),
        "packages/kasumi-plugin-fs/package.json": _json_version(
            root / "packages" / "kasumi-plugin-fs" / "package.json"
        ),
        "packages/kasumi-plugin-dialog/package.json": _json_version(
            root / "packages" / "kasumi-plugin-dialog" / "package.json"
        ),
        "packages/kasumi-plugin-sql/package.json": _json_version(
            root / "packages" / "kasumi-plugin-sql" / "package.json"
        ),
        "kasumi.config.json.windows": _nested_json_version(
            root / "kasumi.config.json",
            "windows",
            "version",
        ),
        "kasumi.config.json.android": _nested_json_version(
            root / "kasumi.config.json",
            "android",
            "version",
        ),
        "windows/vcpkg.json": _json_key_version(
            root / "windows" / "vcpkg.json",
            "version-string",
        ),
        "pyproject.toml": _toml_version(root / "pyproject.toml"),
    }


def check_versions(root: Path = ROOT) -> dict[str, str]:
    version = read_version(root)
    mismatches = {
        name: value
        for name, value in derived_versions(root).items()
        if value != version
    }
    for relative_path in VERSION_SENSITIVE_SOURCES:
        path = root / relative_path
        if path.is_file() and version in path.read_text(encoding="utf-8"):
            mismatches[f"{relative_path} (hardcoded)"] = version
    return mismatches


def sync_versions(root: Path = ROOT) -> list[str]:
    version = read_version(root)
    changed: list[str] = []
    package_paths = [
        root / "package.json",
        root / "packages" / "kasumi-bridge" / "package.json",
        root / "packages" / "kasumi-cli" / "package.json",
        root / "packages" / "create-kasumi" / "package.json",
        root / "packages" / "kasumi-plugin-fs" / "package.json",
        root / "packages" / "kasumi-plugin-dialog" / "package.json",
        root / "packages" / "kasumi-plugin-sql" / "package.json",
    ]
    for path in package_paths:
        if _set_json_value(path, ["version"], version):
            changed.append(_relative(path, root))

    config_path = root / "kasumi.config.json"
    config_changed = False
    if _set_json_value(config_path, ["windows", "version"], version):
        config_changed = True
    if _set_json_value(config_path, ["android", "version"], version):
        config_changed = True
    if config_changed:
        changed.append(_relative(config_path, root))

    vcpkg_path = root / "windows" / "vcpkg.json"
    if _set_json_value(vcpkg_path, ["version-string"], version):
        changed.append(_relative(vcpkg_path, root))

    pyproject_path = root / "pyproject.toml"
    if _set_toml_value(pyproject_path, "version", version):
        changed.append(_relative(pyproject_path, root))
    return changed


def _read_json(path: Path) -> dict[str, Any]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise VersionMetadataError(f"Expected a JSON object: {path}")
    return payload


def _write_json(path: Path, payload: dict[str, Any]) -> None:
    content = json.dumps(payload, ensure_ascii=False, indent=2) + "\n"
    if path.read_text(encoding="utf-8") != content:
        path.write_text(content, encoding="utf-8", newline="\n")


def _set_json_value(path: Path, keys: list[str], value: str) -> bool:
    payload = _read_json(path)
    target: dict[str, Any] = payload
    for key in keys[:-1]:
        child = target.get(key)
        if not isinstance(child, dict):
            raise VersionMetadataError(f"Missing object key {key} in {path}")
        target = child
    final_key = keys[-1]
    if target.get(final_key) == value:
        return False
    target[final_key] = value
    _write_json(path, payload)
    return True


def _json_version(path: Path) -> str:
    return str(_read_json(path).get("version", ""))


def _json_key_version(path: Path, key: str) -> str:
    return str(_read_json(path).get(key, ""))


def _nested_json_version(path: Path, *keys: str) -> str:
    value: Any = _read_json(path)
    for key in keys:
        if not isinstance(value, dict):
            return ""
        value = value.get(key)
    return str(value)


def _toml_version(path: Path) -> str:
    match = re.search(
        r'(?m)^version\s*=\s*"([^"]+)"\s*$',
        path.read_text(encoding="utf-8"),
    )
    return match.group(1) if match is not None else ""


def _set_toml_value(path: Path, key: str, value: str) -> bool:
    content = path.read_text(encoding="utf-8")
    pattern = re.compile(rf'(?m)^{re.escape(key)}\s*=\s*"[^"]+"\s*$')
    replacement = f'{key} = "{value}"'
    updated, count = pattern.subn(replacement, content, count=1)
    if count == 0:
        raise VersionMetadataError(f"Unable to update {key} in {path}")
    if updated == content:
        return False
    path.write_text(updated, encoding="utf-8", newline="\n")
    return True


def _relative(path: Path, root: Path) -> str:
    return path.relative_to(root).as_posix()
