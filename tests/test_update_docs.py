from __future__ import annotations

import io
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

from tools.update_docs import (
    DOCS_DIR,
    MAX_DOC_BYTES,
    SKILL_DOCS_DIR,
    DocsError,
    create_page,
    default_doc_path,
    denormalize,
    ensure_docs_output_path,
    normalize,
    parse_env_file,
    publish,
    read_text,
    split_frontmatter,
    validate_base_url,
    validate_doc_path,
)


class FakeClient:
    def __init__(self, online: str, *, exists: bool = True) -> None:
        self.online = online
        self.exists = exists
        self.submissions: list[dict[str, str]] = []

    def raw(self, _path: str) -> str:
        return self.online

    def try_raw(self, _path: str) -> str | None:
        return self.online if self.exists else None

    def page(self, _path: str) -> dict[str, str]:
        return {
            "title": "Development",
            "description": "Current status",
        }

    def submit(
        self,
        path: str,
        title: str,
        description: str,
        content: str,
        note: str,
        endpoint: str = "/agent/docs/revisions",
    ) -> dict[str, object]:
        self.submissions.append(
            {
                "path": path,
                "title": title,
                "description": description,
                "content": content,
                "note": note,
                "endpoint": endpoint,
            }
        )
        return {
            "code": 200,
            "data": {"uuid": "revision-id", "status": "pending"},
        }


class DocPathTest(unittest.TestCase):
    def test_accepts_kasumi_paths(self) -> None:
        for path in (
            "kasumi",
            "kasumi/development",
            "kasumi/api/types",
            "kasumi/plugins/sql",
            "skill/kasumi/framework",
            "skill/kasumi/app",
        ):
            with self.subTest(path=path):
                self.assertEqual(validate_doc_path(path), path)

    def test_rejects_paths_outside_kasumi(self) -> None:
        for path in (
            "",
            "/kasumi",
            "other/index",
            "kasumi/../release",
            r"kasumi\\development",
            "kasumi/Development",
            "skill/other/app",
        ):
            with self.subTest(path=path):
                with self.assertRaises(DocsError):
                    validate_doc_path(path)

    def test_default_output_stays_inside_tmp(self) -> None:
        path = default_doc_path("kasumi/plugins/sql")
        self.assertEqual(path.suffix, ".md")
        path.relative_to(DOCS_DIR.resolve(strict=False))

    def test_output_outside_docs_tmp_is_rejected(self) -> None:
        with self.assertRaises(DocsError):
            ensure_docs_output_path(Path(".tmp/not-docs.md"))
        with self.assertRaises(DocsError):
            ensure_docs_output_path(Path("DEVELOPMENT.md"))

    def test_output_accepts_repository_skill_docs(self) -> None:
        path = ensure_docs_output_path(
            Path("docs/skill/kasumi/framework.mdx")
        )
        path.relative_to(SKILL_DOCS_DIR.resolve(strict=False))

    def test_output_rejects_other_repository_docs(self) -> None:
        with self.assertRaises(DocsError):
            ensure_docs_output_path(Path("docs/guide/framework.mdx"))


class EnvironmentTest(unittest.TestCase):
    def test_accepts_https_base_url(self) -> None:
        self.assertEqual(
            validate_base_url("https://api.example.com/"),
            "https://api.example.com",
        )

    def test_accepts_loopback_http_for_local_tests(self) -> None:
        self.assertEqual(
            validate_base_url("http://127.0.0.1:8080"),
            "http://127.0.0.1:8080",
        )

    def test_rejects_unsafe_base_urls(self) -> None:
        for value in (
            "http://api.example.com",
            "ftp://api.example.com",
            "https://user@api.example.com",
            "https://api.example.com:invalid",
            "https://api.example.com?token=1",
            "https://api.example.com/base",
        ):
            with self.subTest(value=value):
                with self.assertRaises(DocsError):
                    validate_base_url(value)

    def test_parses_env_file(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / ".env"
            path.write_text(
                "\n".join(
                    (
                        "# comment",
                        "export MIOMOE_AGENT_BASE_URL='https://api.example.com'",
                        'MIOMOE_AGENT_KEY="secret"',
                    )
                ),
                encoding="utf-8",
            )
            values = parse_env_file(path)
        self.assertEqual(values["MIOMOE_AGENT_BASE_URL"], "https://api.example.com")
        self.assertEqual(values["MIOMOE_AGENT_KEY"], "secret")


class ContentTest(unittest.TestCase):
    def test_normalizes_line_endings(self) -> None:
        self.assertEqual(normalize("a\r\nb"), "a\nb")
        self.assertEqual(denormalize("a\nb", True), "a\r\nb")

    def test_rejects_oversized_files(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "large.md"
            path.write_bytes(b"a" * (MAX_DOC_BYTES + 1))
            with self.assertRaises(DocsError):
                read_text(path)

    def test_dry_run_never_submits(self) -> None:
        client = FakeClient("old\n")
        output = io.StringIO()
        with redirect_stdout(output):
            publish(
                client,
                path="kasumi/development",
                content="new\n",
                note="",
                description=None,
                dry_run=True,
                yes=False,
            )
        self.assertEqual(client.submissions, [])
        self.assertIn("--- online:kasumi/development", output.getvalue())

    def test_actual_submission_requires_note(self) -> None:
        client = FakeClient("old\n")
        with redirect_stdout(io.StringIO()):
            with self.assertRaisesRegex(DocsError, "--note"):
                publish(
                    client,
                    path="kasumi/development",
                    content="new\n",
                    note="",
                    description=None,
                    dry_run=False,
                    yes=True,
                )
        self.assertEqual(client.submissions, [])

    def test_submission_preserves_online_line_endings(self) -> None:
        client = FakeClient("old\r\n")
        with redirect_stdout(io.StringIO()):
            publish(
                client,
                path="kasumi/development",
                content="new\n",
                note="update development status",
                description=None,
                dry_run=False,
                yes=True,
            )
        self.assertEqual(len(client.submissions), 1)
        self.assertEqual(client.submissions[0]["content"], "new\r\n")
        self.assertEqual(
            client.submissions[0]["description"],
            "Current status",
        )

    def test_split_frontmatter(self) -> None:
        metadata, body = split_frontmatter(
            '---\ntitle: "Framework"\ndescription: "Rules"\n---\n\n# Heading\n\nBody\n'
        )
        self.assertEqual(metadata["title"], "Framework")
        self.assertEqual(metadata["description"], "Rules")
        self.assertEqual(body, "# Heading\n\nBody\n")

    def test_create_dry_run_strips_h1_and_keeps_frontmatter(self) -> None:
        client = FakeClient("", exists=False)
        output = io.StringIO()
        with redirect_stdout(output):
            create_page(
                client,
                path="kasumi/skills/framework",
                content="# Kasumi Framework\n\nRules\n",
                title="Kasumi Framework",
                description="Framework rules",
                note="",
                dry_run=True,
                yes=False,
            )
        self.assertEqual(client.submissions, [])
        self.assertIn('title: "Kasumi Framework"', output.getvalue())
        self.assertNotIn("# Kasumi Framework", output.getvalue())
        self.assertIn("Rules", output.getvalue())

    def test_create_rejects_existing_page(self) -> None:
        client = FakeClient("old")
        with self.assertRaisesRegex(DocsError, "已存在"):
            create_page(
                client,
                path="kasumi/skills/framework",
                content="Rules\n",
                title="Kasumi Framework",
                description="Framework rules",
                note="create skill",
                dry_run=False,
                yes=True,
            )

    def test_create_uses_document_endpoint(self) -> None:
        client = FakeClient("", exists=False)
        with redirect_stdout(io.StringIO()):
            create_page(
                client,
                path="kasumi/skills/framework",
                content="Rules\n",
                title="Kasumi Framework",
                description="Framework rules",
                note="create skill",
                dry_run=False,
                yes=True,
            )
        self.assertEqual(len(client.submissions), 1)
        self.assertEqual(client.submissions[0]["endpoint"], "/agent/docs")


if __name__ == "__main__":
    unittest.main()
