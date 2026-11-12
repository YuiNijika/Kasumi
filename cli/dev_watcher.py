from __future__ import annotations

from pathlib import Path

IGNORED_DIRECTORIES = {
    ".git",
    ".gradle",
    ".idea",
    ".ksm",
    ".tmp",
    ".vs",
    ".vscode",
    "build",
    "CMakeFiles",
    "Debug",
    "dist",
    "install",
    "node_modules",
    "out",
    "packages",
    "Release",
    "vcpkg_installed",
    "vendor",
}
IGNORED_FILE_NAMES = {".DS_Store", "Thumbs.db"}
IGNORED_FILE_SUFFIXES = (
    ".dll",
    ".exe",
    ".ilk",
    ".lastbuildstate",
    ".log",
    ".obj",
    ".pdb",
    ".suo",
    ".swp",
    ".swo",
    ".temp",
    ".tlog",
    ".tmp",
    ".user",
)


def collect_snapshot(
    project_root: Path,
    config_path: Path,
    extra_files: tuple[Path, ...] = (),
) -> dict[str, tuple[float, int]]:
    snapshot: dict[str, tuple[float, int]] = {}
    for directory in (
        project_root / "windows",
        project_root / "examples" / "win32-host",
        project_root / "extensions",
    ):
        _collect_directory(directory, snapshot)
    _collect_file(config_path, snapshot)
    for file_path in extra_files:
        _collect_file(file_path, snapshot)
    return snapshot


def changed_files(
    previous: dict[str, tuple[float, int]],
    current: dict[str, tuple[float, int]],
    project_root: Path,
) -> list[str]:
    paths = {
        path
        for path in previous.keys() | current.keys()
        if previous.get(path) != current.get(path)
    }
    return [
        _display_path(Path(path), project_root)
        for path in sorted(paths)
    ]


def _collect_directory(
    directory: Path,
    snapshot: dict[str, tuple[float, int]],
) -> None:
    try:
        entries = list(directory.iterdir())
    except FileNotFoundError:
        return
    for entry in entries:
        if entry.is_symlink():
            continue
        if entry.is_dir():
            if entry.name not in IGNORED_DIRECTORIES:
                _collect_directory(entry, snapshot)
            continue
        if entry.is_file() and not _is_ignored_file(entry.name):
            _collect_file(entry, snapshot)


def _collect_file(
    file_path: Path,
    snapshot: dict[str, tuple[float, int]],
) -> None:
    try:
        details = file_path.stat()
    except FileNotFoundError:
        return
    if details.st_size >= 0:
        snapshot[str(file_path.resolve())] = (
            details.st_mtime_ns,
            details.st_size,
        )


def _is_ignored_file(name: str) -> bool:
    if name in IGNORED_FILE_NAMES or name.startswith("."):
        return True
    return name.lower().endswith(IGNORED_FILE_SUFFIXES)


def _display_path(file_path: Path, project_root: Path) -> str:
    try:
        return file_path.relative_to(project_root.resolve()).as_posix()
    except ValueError:
        return str(file_path)
