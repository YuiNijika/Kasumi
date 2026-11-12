from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from cli.kasumi_config import ConfigError, load_config
from cli.ksm import read_app_config, resolve_project_icon


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

    def test_accepts_windows_background_color(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                    "window": {"backgroundColor": "#0F1117"},
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                },
            }
        )

        config = load_config(path)

        self.assertEqual(
            config.windows.window["backgroundColor"],
            "#0F1117",
        )

    def test_accepts_windows_error_page_customization(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                    "errorPage": {
                        "backgroundColor": "#0B0B0D",
                        "cardColor": "#15171D",
                        "borderColor": "#2B2F38",
                        "textColor": "#F5F6FA",
                        "mutedTextColor": "#9CA3AF",
                        "accentColor": "#8B5CF6",
                        "errorColor": "#FF6B6B",
                        "title": "Music Storm 启动失败",
                        "hint": "请检查网络、WebView2 运行时或应用配置后重试",
                    },
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                },
            }
        )

        config = load_config(path)

        self.assertEqual(
            config.windows.error_page,
            {
                "backgroundColor": "#0B0B0D",
                "cardColor": "#15171D",
                "borderColor": "#2B2F38",
                "textColor": "#F5F6FA",
                "mutedTextColor": "#9CA3AF",
                "accentColor": "#8B5CF6",
                "errorColor": "#FF6B6B",
                "title": "Music Storm 启动失败",
                "hint": "请检查网络、WebView2 运行时或应用配置后重试",
            },
        )
        self.assertIsNone(config.android.error_page)

    def test_rejects_unsafe_windows_error_page_customization(self) -> None:
        invalid_error_pages = (
            {"backgroundColor": "#12"},
            {"title": '失败"提示'},
            {"hint": "第一行\n第二行"},
            {"unknown": "#FFFFFF"},
        )
        for error_page in invalid_error_pages:
            with self.subTest(error_page=error_page):
                path = self.write_config(
                    {
                        "windows": {
                            "packageName": "kasumi-win32-example",
                            "version": "1.2.3",
                            "commands": ["ping", "capabilities.query"],
                            "errorPage": error_page,
                        },
                        "android": {
                            "packageName": "dev.kasumi.example",
                            "version": "1.2.3",
                            "commands": ["ping", "capabilities.query"],
                        },
                    }
                )
                with self.assertRaisesRegex(
                    ConfigError,
                    "errorPage|safe characters|RRGGBB|unknown fields",
                ):
                    load_config(path)

    def test_rejects_invalid_windows_background_color(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                    "window": {"backgroundColor": "#12"},
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                },
            }
        )

        with self.assertRaisesRegex(
            ConfigError,
            "backgroundColor must be #RRGGBB or #AARRGGBB",
        ):
            load_config(path)

    def test_accepts_windows_window_size_limits(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                    "window": {
                        "width": 1280,
                        "height": 800,
                        "minWidth": 960,
                        "minHeight": 600,
                        "maxWidth": 1920,
                        "maxHeight": 1200,
                    },
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                },
            }
        )

        config = load_config(path)

        self.assertEqual(config.windows.window["width"], 1280)
        self.assertEqual(config.windows.window["height"], 800)
        self.assertEqual(config.windows.window["minWidth"], 960)
        self.assertEqual(config.windows.window["minHeight"], 600)
        self.assertEqual(config.windows.window["maxWidth"], 1920)
        self.assertEqual(config.windows.window["maxHeight"], 1200)

    def test_rejects_inconsistent_windows_window_size_limits(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                    "window": {
                        "width": 1280,
                        "height": 800,
                        "minWidth": 960,
                        "minHeight": 600,
                        "maxWidth": 900,
                        "maxHeight": 1200,
                    },
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                },
            }
        )

        with self.assertRaisesRegex(ConfigError, "maxWidth must be greater"):
            load_config(path)

    def test_window_decorations_and_top_level_override(self) -> None:
        path = self.write_config(
            {
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                    "window": {"decorations": False, "width": 1280},
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
        self.assertEqual(config.windows.window["width"], 1280)

        payload = json.loads(path.read_text(encoding="utf-8"))
        payload["windows"]["decorations"] = True
        path.write_text(json.dumps(payload), encoding="utf-8")
        self.assertTrue(load_config(path).windows.decorations)

    def test_merges_root_icons_into_platforms(self) -> None:
        path = self.write_config(
            {
                "icons": {
                    "window": "icons/app.ico",
                    "tray": "icons/tray.ico",
                    "android": "icons/android.png",
                },
                "windows": {
                    "packageName": "kasumi-win32-example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                    "icons": {"tray": "icons/windows-tray.ico"},
                },
                "android": {
                    "packageName": "dev.kasumi.example",
                    "version": "1.2.3",
                    "commands": ["ping", "capabilities.query"],
                },
            }
        )

        config = load_config(path)

        self.assertEqual(config.windows.icons["tray"], "icons/windows-tray.ico")
        self.assertEqual(config.windows.icons["window"], "icons/app.ico")
        self.assertEqual(config.android.icons["android"], "icons/android.png")
        self.assertEqual(config.android.icons["tray"], "icons/tray.ico")

    def test_resolves_project_icons_safely(self) -> None:
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        root = Path(directory.name)
        icon = root / "icon.ico"
        icon.write_bytes(b"icon")

        self.assertEqual(
            resolve_project_icon(
                root,
                "icon.ico",
                ".ico",
                "configuration.icons.window",
            ),
            icon.resolve(),
        )
        with self.assertRaisesRegex(ConfigError, "must use the .ico extension"):
            resolve_project_icon(
                root,
                "icon.png",
                ".ico",
                "configuration.icons.window",
            )
        with self.assertRaisesRegex(ConfigError, "must stay inside the project"):
            resolve_project_icon(
                root,
                "../outside.ico",
                ".ico",
                "configuration.icons.window",
            )

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

        protocol = json.loads(
            (
                Path(__file__).resolve().parent.parent
                / "protocol"
                / "commands.json"
            ).read_text(encoding="utf-8")
        )
        self.assertEqual(
            len(config.windows.commands),
            sum(
                "windows" in command["platforms"]
                for command in protocol["commands"].values()
            ),
        )
        self.assertEqual(
            len(config.android.commands),
            sum(
                "android" in command["platforms"]
                for command in protocol["commands"].values()
            ),
        )
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
