from __future__ import annotations

import argparse
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from cli.version_metadata import check_versions, read_version


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("version")
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
    )
    args = parser.parse_args()

    expected = read_version(args.root.resolve())
    if args.version != expected:
        print(
            f"release tag version {args.version} does not match "
            f"version.json {expected}",
            file=sys.stderr,
        )
        raise SystemExit(1)

    mismatches = check_versions(args.root.resolve())
    if mismatches:
        for name, value in mismatches.items():
            print(f"{name}: expected {expected}, found {value}", file=sys.stderr)
        raise SystemExit(1)
    print(f"release versions match {expected}")


if __name__ == "__main__":
    main()
