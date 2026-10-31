from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


def json_version(path: Path) -> str:
    return str(json.loads(path.read_text(encoding="utf-8"))["version"])


def first_match(path: Path, pattern: str) -> str:
    match = re.search(pattern, path.read_text(encoding="utf-8"))
    if match is None:
        raise ValueError(f"Unable to read version from {path}")
    return match.group(1)


def versions(root: Path) -> dict[str, str]:
    return {
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
            r"project\(kasumi_windows VERSION ([^\s\)]+)",
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
