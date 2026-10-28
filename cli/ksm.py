from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def run(command: list[str], cwd: Path = ROOT) -> None:
    subprocess.run(command, cwd=cwd, check=True)


def codegen() -> None:
    run([sys.executable, str(ROOT / "tools" / "codegen.py")])

def validate_protocol() -> None:
    run([sys.executable, str(ROOT / "tools" / "validate_protocol.py")])


def build() -> None:
    validate_protocol()
    codegen()
    run(["pnpm", "--dir", "frontend", "run", "typecheck"])
    run(["cmake", "-S", "windows", "-B", "windows/build"])
    run(["cmake", "--build", "windows/build", "--config", "Release"])


def test() -> None:
    validate_protocol()
    codegen()
    run(["pnpm", "--dir", "frontend", "run", "typecheck"])
    run(["cmake", "-S", "windows", "-B", "windows/build", "-DCMAKE_BUILD_TYPE=Release"])
    run(["cmake", "--build", "windows/build", "--config", "Release"])
    run(["ctest", "--test-dir", "windows/build", "-C", "Release", "--output-on-failure"])


def dev() -> None:
    run(["pnpm", "--dir", "frontend", "run", "dev"])


def start() -> None:
    parser = argparse.ArgumentParser(prog="ksm")
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("codegen")
    subparsers.add_parser("validate")
    subparsers.add_parser("build")
    subparsers.add_parser("test")
    subparsers.add_parser("dev")
    args = parser.parse_args()
    if args.command == "validate":
        validate_protocol()
    elif args.command == "codegen":
        codegen()
    elif args.command == "build":
        build()
    elif args.command == "test":
        test()
    else:
        dev()


if __name__ == "__main__":
    start()
