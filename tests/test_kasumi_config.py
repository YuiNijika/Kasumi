from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from cli.kasumi_config import ConfigError, load_config
from cli.ksm import read_app_config


class KasumiConfigTest(unittest.TestCase):
    def write_config(self, payload: object) -> Path:
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        path = Path(directory.name) / "kasumi.config.json"
        path.write_text(json.dumps(payload), encoding="utf-8")
        return path

    def test_loads_platform_package_and_version(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3-beta.1",
                    "commands": ["ping", "capabilities.query", "device.getInfo"],
                },
            }
        )

        config = load_config(path)

        self.assertEqual(config.windows.package_name, "kasumi-win32-example")
        self.assertEqual(config.windows.version, "1.2.3")
        self.assertEqual(
            config.windows.commands,
            ("ping", "capabilities.query"),
        )
        self.assertTrue(config.windows.decorations)
        self.assertEqual(config.android.package_name, "dev.kasumi.example")
        self.assertEqual(config.android.version, "1.2.3-beta.1")
        self.assertEqual(
            config.android.commands,
            ("ping", "capabilities.query", "device.getInfo"),
        )

    def test_accepts_hidden_windows_decorations(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                    "decorations": False,
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                },
            }
        )

        config = load_config(path)

        self.assertFalse(config.windows.decorations)

    def test_rejects_invalid_windows_decorations(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                    "decorations": "false",
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                },
            }
        )

        with self.assertRaisesRegex(ConfigError, "decorations must be a boolean"):
            load_config(path)

    def test_rejects_android_decorations(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                    "decorations": False,
                },
            }
        )

        with self.assertRaisesRegex(ConfigError, "unknown fields: decorations"):
            load_config(path)

    def test_rejects_windows_webview(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                    "webview": {},
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                },
            }
        )

        with self.assertRaisesRegex(ConfigError, "unknown fields: webview"):
            load_config(path)

    def test_expands_all_commands_by_platform(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": "all",
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3",
                    "commands": "all",
                },
            }
        )

        config = load_config(path)

        self.assertEqual(len(config.windows.commands), 102)
        self.assertEqual(len(config.android.commands), 86)
        self.assertNotIn("device.getInfo", config.windows.commands)
        self.assertIn("device.getInfo", config.android.commands)

    def test_loads_default_android_webview_entries(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                },
            }
        )

        config = load_config(path)

        self.assertIsNotNone(config.android.webview)
        assert config.android.webview is not None
        self.assertEqual(
            config.android.webview.production_entry,
            "https://app.local/index.html",
        )
        self.assertEqual(
            config.android.webview.development_entries,
            (
                "http://localhost:5173/index.html",
                "http://localhost:5173/",
                "http://localhost:5173",
            ),
        )
        self.assertEqual(
            config.android.webview.supported_html_files,
            ("index.html",),
        )
        self.assertIsNone(config.windows.webview)

    def test_accepts_android_webview_entries(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                    "webview": {
                        "productionEntry": "https://app.local/start.html",
                        "developmentEntries": [
                            "http://localhost:5173/start.html",
                            "http://localhost:5173/",
                        ],
                        "supportedHtmlFiles": ["index.html", "start.html"],
                    },
                },
            }
        )

        config = load_config(path)

        assert config.android.webview is not None
        self.assertEqual(
            config.android.webview.production_entry,
            "https://app.local/start.html",
        )
        self.assertEqual(
            config.android.webview.supported_html_files,
            ("index.html", "start.html"),
        )

    def test_rejects_unsafe_android_webview_entries(self) -> None:
        invalid_webviews = (
            {"productionEntry": "https://app.local/index.html?next=1"},
            {"productionEntry": "https://app.local/../index.html"},
            {
                "developmentEntries": [
                    "http://localhost:5173/index.html",
                    "http://localhost:5173/",
                ],
                "supportedHtmlFiles": ["other.html"],
            },
            {
                "developmentEntries": [
                    "http://localhost:5173/index.html",
                    "http://localhost:5173/secret.html",
                ]
            },
        )
        for webview in invalid_webviews:
            with self.subTest(webview=webview):
                path = self.write_config(
                    {
                        "windows": {
                            "packageName": "kasumi-win32-example",
                            "version": "1.2.3",
                            "commands": ["ping", "capabilities.query"],
                        },
                        "android": {
                            "packageName": "dev.kasumi.example",
                            "version": "1.2.3",
                            "commands": ["ping", "capabilities.query"],
                            "webview": webview,
                        },
                    }
                )
                with self.assertRaisesRegex(ConfigError, "webview"):
                    load_config(path)

    def test_rejects_missing_platform(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": ["ping"],
                }
            }
        )

        with self.assertRaisesRegex(ConfigError, "android is required"):
            load_config(path)

    def test_rejects_invalid_android_package(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": ["ping"],
                },
                "android": {
                    "packageName": "dev",
                    "version": "1.2.3",
                    "commands": ["ping"],
                },
            }
        )

        with self.assertRaisesRegex(ConfigError, "invalid format"):
            load_config(path)

    def test_rejects_invalid_version(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2",
                    "commands": ["ping"],
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3",
                    "commands": ["ping"],
                },
            }
        )

        with self.assertRaisesRegex(ConfigError, "semantic version"):
            load_config(path)

    def test_rejects_unknown_fields(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": ["ping"],
                    "icon": "icon.ico",
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3",
                    "commands": ["ping"],
                },
            }
        )

        with self.assertRaisesRegex(ConfigError, "unknown fields"):
            load_config(path)

    def test_rejects_empty_commands(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": [],
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                },
            }
        )

        with self.assertRaisesRegex(ConfigError, "non-empty array"):
            load_config(path)

    def test_rejects_duplicate_commands(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": [
                        "ping",
                        "capabilities.query",
                        "ping",
                    ],
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                },
            }
        )

        with self.assertRaisesRegex(ConfigError, "duplicate"):
            load_config(path)

    def test_rejects_unknown_command(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": [
                        "ping",
                        "capabilities.query",
                        "unknown.command",
                    ],
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                },
            }
        )

        with self.assertRaisesRegex(ConfigError, "unknown command"):
            read_app_config(path)

    def test_rejects_command_for_wrong_platform(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": [
                        "ping",
                        "capabilities.query",
                        "device.getInfo",
                    ],
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                },
            }
        )

        with self.assertRaisesRegex(ConfigError, "not available on windows"):
            read_app_config(path)

    def test_requires_core_commands(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": ["ping"],
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                },
            }
        )

        with self.assertRaisesRegex(ConfigError, "core commands"):
            read_app_config(path)


if __name__ == "__main__":
    unittest.main()
