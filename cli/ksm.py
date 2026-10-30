from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PLATFORMS = ("windows", "android")


def run(command: list[str], cwd: Path = ROOT) -> None:
    subprocess.run(command, cwd=cwd, check=True)


def codegen() -> None:
    run([sys.executable, str(ROOT / "tools" / "codegen.py")])


def validate_protocol() -> None:
    run([sys.executable, str(ROOT / "tools" / "validate_protocol.py")])


def frontend_checks(include_contract: bool = False) -> None:
    validate_protocol()
    codegen()
    run(["pnpm", "--dir", "frontend", "run", "typecheck"])
    if include_contract:
        run(["pnpm", "--dir", "frontend", "run", "test:contract"])


def resolve_platforms(platform: str) -> tuple[str, ...]:
    return PLATFORMS if platform == "all" else (platform,)


def require_gradle() -> str:
    executable = shutil.which("gradle")
    if executable is None:
        raise RuntimeError("gradle is required for Android tasks.")
    return executable


def build_windows() -> None:
    configure = [
        "cmake",
        "-S",
        "windows",
        "-B",
        "windows/build",
        "-DCMAKE_BUILD_TYPE=Release",
    ]
    toolchain = os.environ.get("CMAKE_TOOLCHAIN_FILE")
    if toolchain:
        configure.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain}")
    run(configure)
    run(["cmake", "--build", "windows/build", "--config", "Release"])


def build_android() -> None:
    run([require_gradle(), "-p", "android", "assembleDebug"])


def build(platforms: tuple[str, ...]) -> None:
    frontend_checks()
    if "windows" in platforms:
        build_windows()
    if "android" in platforms:
        build_android()


def test_windows() -> None:
    run(["cmake", "-S", "windows", "-B", "windows/build", "-DCMAKE_BUILD_TYPE=Release"])
    run(["cmake", "--build", "windows/build", "--config", "Release"])
    run(["ctest", "--test-dir", "windows/build", "-C", "Release", "--output-on-failure"])


def test_android() -> None:
    run([
        require_gradle(),
        "-p",
        "android",
        "testDebugUnitTest",
        "assembleDebugAndroidTest",
    ])


def test(platforms: tuple[str, ...]) -> None:
    frontend_checks(include_contract=True)
    if "windows" in platforms:
        test_windows()
    if "android" in platforms:
        test_android()


def package_windows() -> None:
    build_windows()
    run([
        "cpack",
        "--config",
        "windows/build/CPackConfig.cmake",
        "-C",
        "Release",
        "-B",
        "windows/build/packages",
    ])


def package_android() -> None:
    run([require_gradle(), "-p", "android", "assembleRelease"])


def package(platforms: tuple[str, ...]) -> None:
    frontend_checks()
    if "windows" in platforms:
        package_windows()
    if "android" in platforms:
        package_android()


def dev() -> None:
    run(["pnpm", "--dir", "frontend", "run", "dev"])


def add_platform_option(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--platform",
        choices=("all", *PLATFORMS),
        default="all",
        help="select the target platform",
    )


def start() -> None:
    parser = argparse.ArgumentParser(prog="ksm")
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("codegen")
    subparsers.add_parser("validate")
    add_platform_option(subparsers.add_parser("build"))
    add_platform_option(subparsers.add_parser("test"))
    add_platform_option(subparsers.add_parser("package"))
    subparsers.add_parser("dev")
    args = parser.parse_args()
    if args.command == "validate":
        validate_protocol()
    elif args.command == "codegen":
        codegen()
    elif args.command == "build":
        build(resolve_platforms(args.platform))
    elif args.command == "test":
        test(resolve_platforms(args.platform))
    elif args.command == "package":
        package(resolve_platforms(args.platform))
    else:
        dev()


if __name__ == "__main__":
    start()
