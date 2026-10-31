from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import time
import urllib.error
import urllib.request
from functools import lru_cache
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PLATFORMS = ("windows", "android")
TARGETS = ("sdk", "example", "all")
DEV_HOST = "127.0.0.1"
DEV_PORT = 5173
DEV_URL = f"http://localhost:{DEV_PORT}/index.html"
SHOWCASE_DIR = ROOT / "examples" / "showcase"
WINDOWS_SDK_BUILD_ROOT = ROOT / "windows" / "build"
WINDOWS_EXAMPLE_DIR = ROOT / "windows" / "examples" / "win32-host"
ANDROID_LOCAL_REPO = ROOT / "android" / "kasumi" / "build" / "repo"
ANDROID_SDK_PACKAGE_DIR = ROOT / "android" / "build" / "packages"
ANDROID_EXAMPLE_PACKAGE_DIR = ROOT / "examples" / "android-host" / "build" / "packages"
WINDOWS_VCPKG_STATIC_TRIPLET = "x64-windows-static-md"
WINDOWS_VCPKG_SHARED_TRIPLET = "x64-windows"


def run(
    command: list[str],
    cwd: Path = ROOT,
    env: dict[str, str] | None = None,
) -> None:
    executable = shutil.which(command[0]) or command[0]
    subprocess.run(
        [executable, *command[1:]],
        cwd=cwd,
        check=True,
        env=env,
    )


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


def resolve_platform_selection(args: argparse.Namespace) -> tuple[str, ...]:
    selected: set[str] = set()
    if args.platform:
        selected.update(resolve_platforms(args.platform))
    if args.windows:
        selected.add("windows")
    if args.android:
        selected.add("android")
    if args.all_platforms:
        selected.update(PLATFORMS)
    if not selected:
        selected.update(resolve_platforms(args.platform_default))
    return tuple(platform for platform in PLATFORMS if platform in selected)


def resolve_targets(target: str) -> tuple[str, ...]:
    return TARGETS[:-1] if target == "all" else (target,)


def android_gradle(project: Path) -> tuple[str, Path]:
    executable = shutil.which("gradle")
    if executable is not None:
        return executable, ROOT
    wrapper = project / ("gradlew.bat" if os.name == "nt" else "gradlew")
    if wrapper.is_file():
        return str(wrapper), project
    raise RuntimeError("gradle is required for Android tasks.")


def java_major_version(home: Path) -> int | None:
    release = home / "release"
    if not release.is_file():
        return None
    for line in release.read_text(encoding="utf-8").splitlines():
        if not line.startswith("JAVA_VERSION="):
            continue
        version = line.partition("=")[2].strip().strip('"')
        major = version.split(".", 1)[0]
        return int(major) if major.isdigit() else None
    return None


def select_android_java_home() -> Path | None:
    configured = os.environ.get("KASUMI_JAVA_HOME")
    if configured and Path(configured).is_dir():
        return Path(configured)

    current = os.environ.get("JAVA_HOME")
    if current:
        current_home = Path(current)
        major = java_major_version(current_home)
        if current_home.is_dir() and major is not None and 17 <= major <= 21:
            return current_home

    candidates: list[Path] = []
    if current:
        candidates.extend(Path(current).parent.glob("jdk-21*"))
    candidates.extend(
        [
            *Path(os.environ.get("ProgramFiles", "")).glob(
                "Eclipse Adoptium/jdk-21*"
            ),
            *Path(os.environ.get("ProgramFiles", "")).glob("Java/jdk-21*"),
            Path(os.environ.get("ProgramFiles", ""))
            / "Android"
            / "Android Studio"
            / "jbr",
        ]
    )
    for candidate in candidates:
        if candidate.is_dir() and java_major_version(candidate) is not None:
            return candidate
    return None


def android_gradle_environment() -> dict[str, str]:
    environment = os.environ.copy()
    gradle_home = ROOT / ".tmp" / "gradle-home"
    gradle_home.mkdir(parents=True, exist_ok=True)
    environment.setdefault("GRADLE_USER_HOME", str(gradle_home))
    java_home = select_android_java_home()
    if java_home:
        environment["JAVA_HOME"] = str(java_home)
        java_bin = str(java_home / "bin")
        environment["PATH"] = java_bin + os.pathsep + environment.get("PATH", "")
    if os.name != "nt" or "JAVA_TOOL_OPTIONS" in environment:
        return environment
    socket_dir = ROOT / ".tmp" / "afunix"
    socket_dir.mkdir(parents=True, exist_ok=True)
    environment["JAVA_TOOL_OPTIONS"] = (
        f"-Djdk.net.unixdomain.tmpdir={socket_dir.resolve()}"
    )
    return environment


@lru_cache(maxsize=1)
def cmake_bin_directory() -> Path | None:
    executable = shutil.which("cmake")
    if executable:
        return Path(executable).resolve().parent

    vswhere_candidates = [
        Path(os.environ.get("ProgramFiles(x86)", ""))
        / "Microsoft Visual Studio"
        / "Installer"
        / "vswhere.exe",
        Path(os.environ.get("ProgramFiles", ""))
        / "Microsoft Visual Studio"
        / "Installer"
        / "vswhere.exe",
    ]
    for vswhere in vswhere_candidates:
        if not vswhere.is_file():
            continue
        result = subprocess.run(
            [
                str(vswhere),
                "-latest",
                "-products",
                "*",
                "-property",
                "installationPath",
            ],
            capture_output=True,
            check=False,
            text=True,
        )
        for line in result.stdout.splitlines():
            installation = Path(line.strip())
            candidate = (
                installation
                / "Common7"
                / "IDE"
                / "CommonExtensions"
                / "Microsoft"
                / "CMake"
                / "CMake"
                / "bin"
            )
            if (candidate / "cmake.exe").is_file():
                return candidate

    standalone = Path(os.environ.get("ProgramFiles", "")) / "CMake" / "bin"
    if (standalone / "cmake.exe").is_file():
        return standalone
    return None


def cmake_tool(name: str) -> str:
    override = os.environ.get(f"{name.upper()}_COMMAND")
    if override:
        return override
    executable_name = f"{name}.exe" if os.name == "nt" else name
    directory = cmake_bin_directory()
    if directory:
        candidate = directory / executable_name
        if candidate.is_file():
            return str(candidate)
    executable = shutil.which(name)
    if executable:
        return executable
    raise RuntimeError(f"{name} is required for Windows builds.")


def resolve_vcpkg_toolchain() -> Path | None:
    configured = os.environ.get("CMAKE_TOOLCHAIN_FILE")
    if configured:
        return Path(configured)

    roots = [
        os.environ.get("VCPKG_ROOT"),
        str(ROOT / ".tmp" / "vcpkg"),
    ]
    for root in roots:
        if not root:
            continue
        candidate = (
            Path(root) / "scripts" / "buildsystems" / "vcpkg.cmake"
        )
        if candidate.is_file():
            return candidate
    return None


def cmake_toolchain_arguments(
    bundle_webview2_loader: bool,
    manifest_mode: bool = False,
    build_directory: Path | None = None,
) -> list[str]:
    arguments: list[str] = []
    toolchain = resolve_vcpkg_toolchain()
    has_cache = (
        build_directory is not None
        and (build_directory / "CMakeCache.txt").is_file()
    )
    if toolchain and not has_cache:
        arguments.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain}")
        arguments.append(
            f"-DVCPKG_MANIFEST_MODE={'ON' if manifest_mode else 'OFF'}"
        )

    configured_triplet = (
        os.environ.get("KASUMI_VCPKG_TRIPLET")
        or os.environ.get("VCPKG_TARGET_TRIPLET")
    )
    if toolchain or configured_triplet:
        triplet = configured_triplet or (
            WINDOWS_VCPKG_SHARED_TRIPLET
            if bundle_webview2_loader
            else WINDOWS_VCPKG_STATIC_TRIPLET
        )
        arguments.append(f"-DVCPKG_TARGET_TRIPLET={triplet}")

    installed_dir = os.environ.get("VCPKG_INSTALLED_DIR")
    if installed_dir:
        arguments.append(f"-DVCPKG_INSTALLED_DIR={installed_dir}")
    else:
        directory_name = (
            "vcpkg-installed-webview2-loader"
            if bundle_webview2_loader
            else "vcpkg-installed"
        )
        local_installed_dir = ROOT / ".tmp" / directory_name
        if toolchain:
            arguments.append(f"-DVCPKG_INSTALLED_DIR={local_installed_dir}")
    return arguments


def windows_sdk_build_directory(bundle_webview2_loader: bool) -> Path:
    name = "sdk-webview2-loader" if bundle_webview2_loader else "sdk"
    return WINDOWS_SDK_BUILD_ROOT / name


def windows_sdk_install_directory(bundle_webview2_loader: bool) -> Path:
    return windows_sdk_build_directory(bundle_webview2_loader) / "install"


def windows_example_build_directory(bundle_webview2_loader: bool) -> Path:
    name = "build-webview2-loader" if bundle_webview2_loader else "build"
    return WINDOWS_EXAMPLE_DIR / name


def build_windows_sdk(bundle_webview2_loader: bool = False) -> None:
    build_directory = windows_sdk_build_directory(bundle_webview2_loader)
    install_directory = windows_sdk_install_directory(bundle_webview2_loader)
    configure = [
        cmake_tool("cmake"),
        "-S",
        str(ROOT / "windows"),
        "-B",
        str(build_directory),
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={install_directory}",
        f"-DKASUMI_BUNDLE_WEBVIEW2_LOADER={'ON' if bundle_webview2_loader else 'OFF'}",
        *cmake_toolchain_arguments(
            bundle_webview2_loader,
            True,
            build_directory,
        ),
    ]
    run(configure)
    run(
        [
            cmake_tool("cmake"),
            "--build",
            str(build_directory),
            "--config",
            "Release",
            "--target",
            "install",
        ]
    )


def build_windows_example(
    external_web: bool = False,
    bundle_webview2_loader: bool = False,
) -> None:
    build_windows_sdk(bundle_webview2_loader)
    build_directory = windows_example_build_directory(bundle_webview2_loader)
    install_directory = windows_sdk_install_directory(bundle_webview2_loader)
    configure = [
        cmake_tool("cmake"),
        "-S",
        str(WINDOWS_EXAMPLE_DIR),
        "-B",
        str(build_directory),
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_PREFIX_PATH={install_directory}",
        f"-DKASUMI_EXAMPLE_EXTERNAL_WEB={'ON' if external_web else 'OFF'}",
        f"-DKASUMI_BUNDLE_WEBVIEW2_LOADER={'ON' if bundle_webview2_loader else 'OFF'}",
        *cmake_toolchain_arguments(
            bundle_webview2_loader,
            False,
            build_directory,
        ),
    ]
    run(configure)
    run(
        [
            cmake_tool("cmake"),
            "--build",
            str(build_directory),
            "--config",
            "Release",
        ]
    )


def build_android_sdk() -> None:
    executable, cwd = android_gradle(ROOT / "android")
    run(
        [
            executable,
            ":kasumi:assembleRelease",
            ":kasumi:publishReleasePublicationToLocalRepository",
        ],
        cwd=cwd,
        env=android_gradle_environment(),
    )


def build_android_example() -> None:
    build_android_sdk()
    project = ROOT / "examples" / "android-host"
    executable, cwd = android_gradle(project)
    run(
        [
            executable,
            "assembleDebug",
        ],
        cwd=cwd,
        env=android_gradle_environment(),
    )


def build(
    platforms: tuple[str, ...],
    targets: tuple[str, ...],
    external_web: bool = False,
    bundle_webview2_loader: bool = False,
) -> None:
    frontend_checks()
    if "windows" in platforms:
        if "sdk" in targets:
            build_windows_sdk(bundle_webview2_loader)
        if "example" in targets:
            build_windows_example(external_web, bundle_webview2_loader)
    if "android" in platforms:
        if "sdk" in targets:
            build_android_sdk()
        if "example" in targets:
            build_android_example()


def test_windows_example(bundle_webview2_loader: bool = False) -> None:
    build_windows_example(False, bundle_webview2_loader)
    run(
        [
            cmake_tool("ctest"),
            "--test-dir",
            str(windows_example_build_directory(bundle_webview2_loader)),
            "-C",
            "Release",
            "--output-on-failure",
        ]
    )


def test_windows(bundle_webview2_loader: bool = False) -> None:
    build_windows_sdk(bundle_webview2_loader)
    run(
        [
            cmake_tool("ctest"),
            "--test-dir",
            str(windows_sdk_build_directory(bundle_webview2_loader)),
            "-C",
            "Release",
            "--output-on-failure",
        ]
    )
    test_windows_example(bundle_webview2_loader)


def test_android() -> None:
    build_android_sdk()
    executable, cwd = android_gradle(ROOT / "android")
    run(
        [
            executable,
            ":kasumi:testDebugUnitTest",
            ":app:testDebugUnitTest",
            ":app:assembleDebugAndroidTest",
        ],
        cwd=cwd,
        env=android_gradle_environment(),
    )
    build_android_example()


def test(
    platforms: tuple[str, ...],
    targets: tuple[str, ...],
    bundle_webview2_loader: bool = False,
) -> None:
    frontend_checks(include_contract=True)
    if "windows" in platforms:
        if "sdk" in targets:
            test_windows(bundle_webview2_loader)
        elif "example" in targets:
            test_windows_example(bundle_webview2_loader)
    if "android" in platforms:
        if "sdk" in targets:
            test_android()
        elif "example" in targets:
            build_android_example()


def package_windows_sdk(bundle_webview2_loader: bool = False) -> None:
    build_windows_sdk(bundle_webview2_loader)
    build_directory = windows_sdk_build_directory(bundle_webview2_loader)
    run(
        [
            cmake_tool("cpack"),
            "--config",
            str(build_directory / "CPackConfig.cmake"),
            "-C",
            "Release",
            "-B",
            str(build_directory / "packages"),
        ]
    )


def windows_example_output_directory(
    bundle_webview2_loader: bool = False,
) -> Path:
    build_directory = windows_example_build_directory(bundle_webview2_loader)
    release = build_directory / "Release"
    return release if release.is_dir() else build_directory


def package_windows_example(
    external_web: bool = False,
    bundle_webview2_loader: bool = False,
) -> None:
    build_windows_example(external_web, bundle_webview2_loader)
    output = windows_example_output_directory(bundle_webview2_loader)
    package_dir = (
        windows_example_build_directory(bundle_webview2_loader) / "packages"
    )
    package_dir.mkdir(parents=True, exist_ok=True)
    staging = package_dir / "kasumi-win32-example-0.1.0"
    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir()

    executable = output / "kasumi_win32_example.exe"
    if not executable.is_file():
        raise FileNotFoundError(f"Windows example executable not found: {executable}")
    shutil.copy2(executable, staging / executable.name)

    if bundle_webview2_loader:
        loader = output / "WebView2Loader.dll"
        if not loader.is_file():
            raise FileNotFoundError(f"WebView2 loader not found: {loader}")
        shutil.copy2(loader, staging / loader.name)

    if external_web:
        web = output / "web"
        if not web.is_dir():
            raise FileNotFoundError(f"External web assets not found: {web}")
        shutil.copytree(web, staging / "web")

    archive = package_dir / "kasumi-win32-example-0.1.0.zip"
    if archive.exists():
        archive.unlink()
    shutil.make_archive(
        str(package_dir / "kasumi-win32-example-0.1.0"),
        "zip",
        root_dir=staging,
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


def package(
    platforms: tuple[str, ...],
    targets: tuple[str, ...],
    external_web: bool = False,
    bundle_webview2_loader: bool = False,
) -> None:
    frontend_checks()
    if "windows" in platforms:
        if "sdk" in targets:
            package_windows_sdk(bundle_webview2_loader)
        if "example" in targets:
            package_windows_example(external_web, bundle_webview2_loader)
    if "android" in platforms:
        if "sdk" in targets:
            package_android_sdk()
        if "example" in targets:
            package_android_example()


def android_adb() -> str:
    candidates = [os.environ.get("ADB"), shutil.which("adb")]
    sdk_roots = [
        os.environ.get("ANDROID_HOME"),
        os.environ.get("ANDROID_SDK_ROOT"),
    ]
    local_properties = ROOT / "android" / "local.properties"
    if local_properties.is_file():
        for line in local_properties.read_text(encoding="utf-8").splitlines():
            if line.startswith("sdk.dir="):
                sdk_roots.append(line.partition("=")[2])
                break
    executable_name = "adb.exe" if os.name == "nt" else "adb"
    for sdk_root in sdk_roots:
        if sdk_root:
            candidates.append(str(Path(sdk_root) / "platform-tools" / executable_name))
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return candidate
    raise RuntimeError("adb is required for Android development.")


def android_adb_environment() -> dict[str, str]:
    environment = os.environ.copy()
    if os.name != "nt":
        return environment
    home = os.path.expanduser("~")
    drive, tail = os.path.splitdrive(home)
    environment["HOME"] = home
    environment["USERPROFILE"] = home
    environment["HOMEDRIVE"] = drive
    environment["HOMEPATH"] = tail
    return environment


def start_dev_server() -> subprocess.Popen[bytes]:
    if not (SHOWCASE_DIR / "index.html").is_file():
        raise FileNotFoundError(f"Showcase entry not found: {SHOWCASE_DIR / 'index.html'}")
    server = subprocess.Popen(
        [
            sys.executable,
            "-m",
            "http.server",
            str(DEV_PORT),
            "--bind",
            DEV_HOST,
            "--directory",
            str(SHOWCASE_DIR),
        ],
        cwd=ROOT,
    )
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        if server.poll() is not None:
            raise RuntimeError("The development server exited before it became ready.")
        try:
            with urllib.request.urlopen(DEV_URL, timeout=0.5):
                return server
        except (OSError, urllib.error.URLError):
            time.sleep(0.2)
    server.terminate()
    raise RuntimeError(f"The development server did not become ready: {DEV_URL}")


def stop_dev_server(server: subprocess.Popen[bytes]) -> None:
    if server.poll() is not None:
        return
    server.terminate()
    try:
        server.wait(timeout=5)
    except subprocess.TimeoutExpired:
        server.kill()
        server.wait()


def install_android_apk(
    adb: str,
    apk: Path,
    environment: dict[str, str],
) -> None:
    result = subprocess.run(
        [adb, "install", "-r", str(apk)],
        capture_output=True,
        env=environment,
        text=True,
    )
    output = f"{result.stdout}{result.stderr}"
    if output:
        print(output, end="")
    if result.returncode == 0:
        return
    if "INSTALL_FAILED_UPDATE_INCOMPATIBLE" not in output:
        raise subprocess.CalledProcessError(
            result.returncode,
            result.args,
            output=output,
        )
    subprocess.run(
        [adb, "uninstall", "dev.kasumi.example"],
        check=False,
        env=environment,
    )
    run([adb, "install", str(apk)], env=environment)


def dev_windows(bundle_webview2_loader: bool = False) -> None:
    build_windows_example(False, bundle_webview2_loader)
    server = start_dev_server()
    try:
        executable = (
            windows_example_output_directory(bundle_webview2_loader)
            / "kasumi_win32_example.exe"
        )
        if not executable.is_file():
            raise FileNotFoundError(f"Windows example executable not found: {executable}")
        print(f"Windows development app is running against {DEV_URL}")
        run([str(executable), "--url", DEV_URL, "--dev"], cwd=executable.parent)
    finally:
        stop_dev_server(server)


def dev_android() -> None:
    build_android_example()
    adb = android_adb()
    environment = android_adb_environment()
    apk = (
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
    if not apk.is_file():
        raise FileNotFoundError(f"Android example APK not found: {apk}")
    run(
        [adb, "reverse", f"tcp:{DEV_PORT}", f"tcp:{DEV_PORT}"],
        env=environment,
    )
    server: subprocess.Popen[bytes] | None = None
    try:
        server = start_dev_server()
        install_android_apk(adb, apk, environment)
        run(
            [adb, "shell", "am", "force-stop", "dev.kasumi.example"],
            env=environment,
        )
        run(
            [
                adb,
                "shell",
                "am",
                "start",
                "-n",
                "dev.kasumi.example/.MainActivity",
                "--es",
                "ksmDevUrl",
                DEV_URL,
            ],
            env=environment,
        )
        print(f"Android development app is running against {DEV_URL}")
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            pass
    finally:
        if server:
            stop_dev_server(server)
        subprocess.run(
            [adb, "shell", "am", "force-stop", "dev.kasumi.example"],
            check=False,
            env=environment,
        )
        subprocess.run(
            [adb, "reverse", "--remove", f"tcp:{DEV_PORT}"],
            check=False,
            env=environment,
        )


def dev(
    platforms: tuple[str, ...],
    bundle_webview2_loader: bool = False,
) -> None:
    if len(platforms) != 1:
        raise RuntimeError("dev requires exactly one platform.")
    frontend_checks()
    if platforms[0] == "windows":
        dev_windows(bundle_webview2_loader)
        return
    dev_android()


def add_platform_option(
    parser: argparse.ArgumentParser,
    default: str = "all",
) -> None:
    parser.add_argument(
        "--platform",
        choices=("all", *PLATFORMS),
        default=None,
        help="select the target platform",
    )
    parser.add_argument(
        "--windows",
        action="store_true",
        help="select the Windows platform",
    )
    parser.add_argument(
        "--android",
        action="store_true",
        help="select the Android platform",
    )
    parser.add_argument(
        "--all",
        dest="all_platforms",
        action="store_true",
        help="select Windows and Android",
    )
    parser.set_defaults(platform_default=default)


def add_target_option(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--target",
        choices=TARGETS,
        default="all",
        help="select SDK, example, or both",
    )


def add_external_web_option(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--external-web",
        action="store_true",
        help="build or package the Windows example with external web assets",
    )


def add_webview2_option(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--with-webview2-loader",
        dest="bundle_webview2_loader",
        action="store_true",
        help=(
            "Windows only: bundle WebView2Loader.dll instead of statically "
            "linking the system WebView2 loader"
        ),
    )


def start() -> None:
    parser = argparse.ArgumentParser(prog="ksm")
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("codegen")
    subparsers.add_parser("validate")

    build_parser = subparsers.add_parser(
        "build",
        description="Build Windows by default. Use --android or --all for other targets.",
    )
    add_platform_option(build_parser, default="windows")
    add_target_option(build_parser)
    add_external_web_option(build_parser)
    add_webview2_option(build_parser)

    test_parser = subparsers.add_parser("test")
    add_platform_option(test_parser)
    add_target_option(test_parser)
    add_webview2_option(test_parser)

    package_parser = subparsers.add_parser("package")
    add_platform_option(package_parser)
    add_target_option(package_parser)
    add_external_web_option(package_parser)
    add_webview2_option(package_parser)

    dev_parser = subparsers.add_parser(
        "dev",
        description="Run the Windows development host by default.",
    )
    add_platform_option(dev_parser, default="windows")
    add_webview2_option(dev_parser)
    args = parser.parse_args()

    if args.command == "validate":
        validate_protocol()
    elif args.command == "codegen":
        codegen()
    elif args.command == "build":
        build(
            resolve_platform_selection(args),
            resolve_targets(args.target),
            args.external_web,
            args.bundle_webview2_loader,
        )
    elif args.command == "test":
        test(
            resolve_platform_selection(args),
            resolve_targets(args.target),
            args.bundle_webview2_loader,
        )
    elif args.command == "package":
        package(
            resolve_platform_selection(args),
            resolve_targets(args.target),
            args.external_web,
            args.bundle_webview2_loader,
        )
    else:
        dev(
            resolve_platform_selection(args),
            args.bundle_webview2_loader,
        )


if __name__ == "__main__":
    start()
