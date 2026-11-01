from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


SEMVER_PATTERN = re.compile(
    r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)"
    r"(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?"
    r"(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$"
)


def json_version(path: Path) -> str:
    return str(json.loads(path.read_text(encoding="utf-8"))["version"])


def nested_json_version(path: Path, *keys: str) -> str:
    value: object = json.loads(path.read_text(encoding="utf-8"))
    for key in keys:
        if not isinstance(value, dict) or key not in value:
            raise ValueError(f"Unable to read version from {path}")
        value = value[key]
    return str(value)


def first_match(path: Path, pattern: str) -> str:
    match = re.search(pattern, path.read_text(encoding="utf-8"))
    if match is None:
        raise ValueError(f"Unable to read version from {path}")
    return match.group(1)


def versions(root: Path) -> dict[str, str]:
    return {
        "root": json_version(root / "package.json"),
        "create-kasumi": json_version(
            root / "packages" / "create-kasumi" / "package.json"
        ),
        "kasumi-cli": json_version(
            root / "packages" / "kasumi-cli" / "package.json"
        ),
        "kasumi-bridge": json_version(
            root / "packages" / "kasumi-bridge" / "package.json"
        ),
        "windows-sdk": first_match(
            root / "windows" / "CMakeLists.txt",
            r'set\(KASUMI_WINDOWS_SDK_VERSION\s+"([^"]+)"\s*\)',
        ),
        "windows-vcpkg": first_match(
            root / "windows" / "vcpkg.json",
            r'"version-string"\s*:\s*"([^"]+)"',
        ),
        "android-sdk": first_match(
            root / "android" / "kasumi" / "build.gradle.kts",
            r'version\s*=\s*"([^"]+)"',
        ),
        "android-example": first_match(
            root / "android" / "app" / "build.gradle.kts",
            r'versionName\s*=\s*"([^"]+)"',
        ),
        "config-windows": nested_json_version(
            root / "kasumi.config.json", "windows", "version"
        ),
        "config-android": nested_json_version(
            root / "kasumi.config.json", "android", "version"
        ),
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("version")
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
    )
    args = parser.parse_args()

    if SEMVER_PATTERN.fullmatch(args.version) is None:
        print(f"invalid release version: {args.version}", file=sys.stderr)
        raise SystemExit(1)

    mismatches = {
        name: value
        for name, value in versions(args.root.resolve()).items()
        if value != args.version
    }
    if mismatches:
        for name, value in mismatches.items():
            print(f"{name}: expected {args.version}, found {value}", file=sys.stderr)
        raise SystemExit(1)
    print(f"release versions match {args.version}")


if __name__ == "__main__":
    main()
