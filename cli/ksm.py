from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PLATFORMS = ("windows", "android")
TARGETS = ("sdk", "example", "all")
WINDOWS_SDK_BUILD = ROOT / "windows" / "build" / "sdk"
WINDOWS_SDK_INSTALL = WINDOWS_SDK_BUILD / "install"
WINDOWS_EXAMPLE_DIR = ROOT / "windows" / "examples" / "win32-host"
WINDOWS_EXAMPLE_BUILD = WINDOWS_EXAMPLE_DIR / "build"
ANDROID_LOCAL_REPO = ROOT / "android" / "kasumi" / "build" / "repo"
ANDROID_SDK_PACKAGE_DIR = ROOT / "android" / "build" / "packages"
ANDROID_EXAMPLE_PACKAGE_DIR = ROOT / "examples" / "android-host" / "build" / "packages"


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


def resolve_targets(target: str) -> tuple[str, ...]:
    return TARGETS[:-1] if target == "all" else (target,)


def require_gradle() -> str:
    executable = shutil.which("gradle")
    if executable is None:
        raise RuntimeError("gradle is required for Android tasks.")
    return executable


def cmake_toolchain_arguments() -> list[str]:
    toolchain = os.environ.get("CMAKE_TOOLCHAIN_FILE")
    if not toolchain:
        return []
    return [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"]


def build_windows_sdk() -> None:
    configure = [
        "cmake",
        "-S",
        str(ROOT / "windows"),
        "-B",
        str(WINDOWS_SDK_BUILD),
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={WINDOWS_SDK_INSTALL}",
        *cmake_toolchain_arguments(),
    ]
    run(configure)
    run(
        [
            "cmake",
            "--build",
            str(WINDOWS_SDK_BUILD),
            "--config",
            "Release",
            "--target",
            "install",
        ]
    )


def build_windows_example() -> None:
    build_windows_sdk()
    configure = [
        "cmake",
        "-S",
        str(WINDOWS_EXAMPLE_DIR),
        "-B",
        str(WINDOWS_EXAMPLE_BUILD),
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_PREFIX_PATH={WINDOWS_SDK_INSTALL}",
        *cmake_toolchain_arguments(),
    ]
    run(configure)
    run(
        [
            "cmake",
            "--build",
            str(WINDOWS_EXAMPLE_BUILD),
            "--config",
            "Release",
        ]
    )


def build_android_sdk() -> None:
    run(
        [
            require_gradle(),
            "-p",
            str(ROOT / "android"),
            ":kasumi:assembleRelease",
            ":kasumi:publishReleasePublicationToLocalRepository",
        ]
    )


def build_android_example() -> None:
    build_android_sdk()
    run(
        [
            require_gradle(),
            "-p",
            str(ROOT / "examples" / "android-host"),
            "assembleDebug",
        ]
    )


def build(platforms: tuple[str, ...], targets: tuple[str, ...]) -> None:
    frontend_checks()
    if "windows" in platforms:
        if "sdk" in targets:
            build_windows_sdk()
        if "example" in targets:
            build_windows_example()
    if "android" in platforms:
        if "sdk" in targets:
            build_android_sdk()
        if "example" in targets:
            build_android_example()


def test_windows() -> None:
    build_windows_sdk()
    run(
        [
            "ctest",
            "--test-dir",
            str(WINDOWS_SDK_BUILD),
            "-C",
            "Release",
            "--output-on-failure",
        ]
    )
    build_windows_example()
    run(
        [
            "ctest",
            "--test-dir",
            str(WINDOWS_EXAMPLE_BUILD),
            "-C",
            "Release",
            "--output-on-failure",
        ]
    )


def test_android() -> None:
    build_android_sdk()
    run(
        [
            require_gradle(),
            "-p",
            str(ROOT / "android"),
            ":kasumi:testDebugUnitTest",
            ":app:testDebugUnitTest",
            ":app:assembleDebugAndroidTest",
        ]
    )
    build_android_example()


def test(platforms: tuple[str, ...], targets: tuple[str, ...]) -> None:
    frontend_checks(include_contract=True)
    if "windows" in platforms:
        if "sdk" in targets:
            test_windows()
        elif "example" in targets:
            build_windows_example()
    if "android" in platforms:
        if "sdk" in targets:
            test_android()
        elif "example" in targets:
            build_android_example()


def package_windows_sdk() -> None:
    build_windows_sdk()
    run(
        [
            "cpack",
            "--config",
            str(WINDOWS_SDK_BUILD / "CPackConfig.cmake"),
            "-C",
            "Release",
            "-B",
            str(WINDOWS_SDK_BUILD / "packages"),
        ]
    )


def package_windows_example() -> None:
    build_windows_example()
    package_dir = WINDOWS_EXAMPLE_BUILD / "packages"
    package_dir.mkdir(parents=True, exist_ok=True)
    archive = package_dir / "kasumi-win32-example-0.1.0"
    if archive.with_suffix(".zip").exists():
        archive.with_suffix(".zip").unlink()
    shutil.make_archive(
        str(archive),
        "zip",
        root_dir=WINDOWS_EXAMPLE_BUILD,
        base_dir="Release",
    )


def package_android_sdk() -> None:
    build_android_sdk()
    ANDROID_SDK_PACKAGE_DIR.mkdir(parents=True, exist_ok=True)
    source = (
        ROOT
        / "android"
        / "kasumi"
        / "build"
        / "outputs"
        / "aar"
        / "kasumi-release.aar"
    )
    shutil.copy2(
        source,
        ANDROID_SDK_PACKAGE_DIR / "kasumi-android-sdk-0.1.0.aar",
    )


def package_android_example() -> None:
    build_android_example()
    ANDROID_EXAMPLE_PACKAGE_DIR.mkdir(parents=True, exist_ok=True)
    source = (
        ROOT
        / "examples"
        / "android-host"
        / "app"
        / "build"
        / "outputs"
        / "apk"
        / "debug"
        / "app-debug.apk"
    )
    shutil.copy2(
        source,
        ANDROID_EXAMPLE_PACKAGE_DIR / "kasumi-android-example-0.1.0.apk",
    )


def package(platforms: tuple[str, ...], targets: tuple[str, ...]) -> None:
    frontend_checks()
    if "windows" in platforms:
        if "sdk" in targets:
            package_windows_sdk()
        if "example" in targets:
            package_windows_example()
    if "android" in platforms:
        if "sdk" in targets:
            package_android_sdk()
        if "example" in targets:
            package_android_example()


def dev() -> None:
    run(["pnpm", "--dir", "frontend", "run", "dev"])


def add_platform_option(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--platform",
        choices=("all", *PLATFORMS),
        default="all",
        help="select the target platform",
    )


def add_target_option(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--target",
        choices=TARGETS,
        default="all",
        help="select SDK, example, or both",
    )


def start() -> None:
    parser = argparse.ArgumentParser(prog="ksm")
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("codegen")
    subparsers.add_parser("validate")

    build_parser = subparsers.add_parser("build")
    add_platform_option(build_parser)
    add_target_option(build_parser)

    test_parser = subparsers.add_parser("test")
    add_platform_option(test_parser)
    add_target_option(test_parser)

    package_parser = subparsers.add_parser("package")
    add_platform_option(package_parser)
    add_target_option(package_parser)

    subparsers.add_parser("dev")
    args = parser.parse_args()

    if args.command == "validate":
        validate_protocol()
    elif args.command == "codegen":
        codegen()
    elif args.command == "build":
        build(
            resolve_platforms(args.platform),
            resolve_targets(args.target),
        )
    elif args.command == "test":
        test(
            resolve_platforms(args.platform),
            resolve_targets(args.target),
        )
    elif args.command == "package":
        package(
            resolve_platforms(args.platform),
            resolve_targets(args.target),
        )
    else:
        dev()


if __name__ == "__main__":
    start()
