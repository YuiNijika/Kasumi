from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def asset(path: Path) -> dict[str, object]:
    if not path.is_file():
        raise FileNotFoundError(f"Release asset not found: {path}")
    return {
        "file": path.name,
        "sha256": sha256(path),
        "size": path.stat().st_size,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--windows-sdk", type=Path, required=True)
    parser.add_argument("--android-maven", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    manifest = {
        "version": args.version,
        "assets": {
            "windowsSdk": asset(args.windows_sdk),
            "androidMaven": asset(args.android_maven),
        },
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
