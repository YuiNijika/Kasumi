from __future__ import annotations

import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from cli.ksm import android_gradle


class AndroidGradleTest(unittest.TestCase):
    def test_prefers_project_wrapper_over_system_gradle(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)
            wrapper = project / ("gradlew.bat" if os.name == "nt" else "gradlew")
            wrapper.write_text("", encoding="utf-8")

            with patch.dict(os.environ, {}, clear=True), patch(
                "cli.ksm.shutil.which",
                return_value="C:/tools/gradle",
            ):
                command, cwd = android_gradle(project)

        expected = [str(wrapper)] if os.name == "nt" else ["sh", str(wrapper)]
        self.assertEqual(command, expected)
        self.assertEqual(cwd, project)

    def test_uses_gradle_environment_override(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)

            with patch.dict(os.environ, {"GRADLE": "/custom/gradle"}, clear=True):
                command, cwd = android_gradle(project)

        self.assertEqual(command, ["/custom/gradle"])
        self.assertEqual(cwd, project)

    def test_falls_back_to_system_gradle_without_wrapper(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)

            with patch.dict(os.environ, {}, clear=True), patch(
                "cli.ksm.shutil.which",
                return_value="/usr/bin/gradle",
            ):
                command, cwd = android_gradle(project)

        self.assertEqual(command, ["/usr/bin/gradle"])
        self.assertEqual(cwd, project)


if __name__ == "__main__":
    unittest.main()
