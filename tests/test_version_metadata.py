from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from cli.version_metadata import (
    ROOT,
    VERSION_SENSITIVE_SOURCES,
    VersionMetadataError,
    check_versions,
    read_version,
    sync_versions,
    validate_version,
)


class VersionMetadataTest(unittest.TestCase):
    def test_current_versions_match(self) -> None:
        self.assertEqual(check_versions(ROOT), {})

    def test_rejects_invalid_semver(self) -> None:
        with self.assertRaises(VersionMetadataError):
            validate_version("26.11")

    def test_sync_updates_derived_manifests(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self._write_fixture(root, "1.0.0", "9.9.9")

            changed = sync_versions(root)

            self.assertIn("package.json", changed)
            self.assertIn("kasumi.config.json", changed)
            self.assertIn("windows/vcpkg.json", changed)
            self.assertIn("pyproject.toml", changed)
            self.assertEqual(check_versions(root), {})
            self.assertEqual(read_version(root), "1.0.0")

    def test_build_sources_do_not_hardcode_release_version(self) -> None:
        version = read_version(ROOT)
        for relative_path in VERSION_SENSITIVE_SOURCES:
            with self.subTest(path=relative_path):
                self.assertNotIn(
                    version,
                    (ROOT / relative_path).read_text(encoding="utf-8"),
                )

    def _write_fixture(
        self,
        root: Path,
        version: str,
        old_version: str,
    ) -> None:
        package_paths = [
            root / "package.json",
            root / "packages" / "kasumi-bridge" / "package.json",
            root / "packages" / "kasumi-cli" / "package.json",
            root / "packages" / "create-kasumi" / "package.json",
        ]
        for path in package_paths:
            path.parent.mkdir(parents=True, exist_ok=True)
            self._write_json(path, {"version": old_version})
        self._write_json(
            root / "version.json",
            {"version": version},
        )
        self._write_json(
            root / "kasumi.config.json",
            {
                "windows": {"version": old_version},
                "android": {"version": old_version},
            },
        )
        vcpkg = root / "windows" / "vcpkg.json"
        vcpkg.parent.mkdir(parents=True, exist_ok=True)
        self._write_json(vcpkg, {"version-string": old_version})
        (root / "pyproject.toml").write_text(
            '[project]\nname = "kasumi-cli"\nversion = "' + old_version + '"\n',
            encoding="utf-8",
        )

    def _write_json(self, path: Path, payload: dict[str, object]) -> None:
        path.write_text(
            json.dumps(payload, indent=2) + "\n",
            encoding="utf-8",
        )


if __name__ == "__main__":
    unittest.main()
