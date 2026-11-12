from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from cli.dev_watcher import changed_files, collect_snapshot


class DevWatcherTest(unittest.TestCase):
    def test_collects_backend_inputs_and_ignores_generated_directories(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            config = root / "kasumi.config.json"
            source = root / "windows" / "src" / "main.cpp"
            generated = root / "windows" / "build" / "generated.cpp"
            manifest = root / "extensions" / "example" / "manifest.json"
            for file_path in (config, source, generated, manifest):
                file_path.parent.mkdir(parents=True, exist_ok=True)
                file_path.write_text("{}\n", encoding="utf-8")

            snapshot = collect_snapshot(root, config, (manifest,))

            self.assertIn(str(config.resolve()), snapshot)
            self.assertIn(str(source.resolve()), snapshot)
            self.assertIn(str(manifest.resolve()), snapshot)
            self.assertNotIn(str(generated.resolve()), snapshot)

    def test_reports_changed_files_relative_to_project(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            config = root / "kasumi.config.json"
            source = root / "windows" / "main.cpp"
            config.write_text("{}\n", encoding="utf-8")
            source.parent.mkdir(parents=True, exist_ok=True)
            source.write_text("v1\n", encoding="utf-8")
            previous = collect_snapshot(root, config)

            source.write_text("version 2\n", encoding="utf-8")
            config.unlink()
            current = collect_snapshot(root, config)

            self.assertEqual(
                changed_files(previous, current, root),
                ["kasumi.config.json", "windows/main.cpp"],
            )


if __name__ == "__main__":
    unittest.main()
