#!/usr/bin/env python3
"""Kasumi 在线文档读写工具。

凭据只从环境变量或显式指定的 env 文件读取。拉取、普通提交和 draft
编辑都限制在 .tmp/kasumi-docs，不再使用 docs/skill/kasumi 本地镜像。
"""

from __future__ import annotations

import argparse
import difflib
import json
import os
import re
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Sequence


ROOT = Path(__file__).resolve().parents[1]
DOCS_DIR = ROOT / ".tmp" / "kasumi-docs"
DOC_PATH_PATTERN = re.compile(r"^kasumi(?:/[a-z0-9][a-z0-9-]*)*$")
SKILL_DOC_PATH_PATTERN = re.compile(
    r"^skill/kasumi(?:/[a-z0-9][a-z0-9-]*)*$"
)

MAX_DOC_BYTES = 4 * 1024 * 1024
MAX_RESPONSE_BYTES = 8 * 1024 * 1024
REQUEST_TIMEOUT_SECONDS = 45


class DocsError(RuntimeError):
    """可展示给用户的文档工具错误。"""


@dataclass(frozen=True)
class Environment:
    base_url: str
    api_key: str


class NoRedirectHandler(urllib.request.HTTPRedirectHandler):
    def redirect_request(
        self,
        request: urllib.request.Request,
        file_pointer: Any,
        code: int,
        message: str,
        headers: Any,
        new_url: str,
    ) -> None:
        raise DocsError(f"拒绝 Agent API 重定向: HTTP {code} {new_url}")


def configure_stdio() -> None:
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except (AttributeError, OSError):
        pass


def is_allowed_doc_path(path: str) -> bool:
    return bool(
        DOC_PATH_PATTERN.fullmatch(path)
        or SKILL_DOC_PATH_PATTERN.fullmatch(path)
    )


def validate_doc_path(path: str) -> str:
    if not path or path != path.strip():
        raise DocsError("文档路径不能为空，也不能包含首尾空格")
    if "\\" in path or "\x00" in path:
        raise DocsError("文档路径不能包含反斜杠或空字符")
    if not is_allowed_doc_path(path):
        raise DocsError(
            "文档路径必须位于 kasumi 或 skill/kasumi 分区"
        )
    return path


def ensure_docs_output_path(path: Path) -> Path:
    candidate = path if path.is_absolute() else ROOT / path
    resolved = candidate.resolve(strict=False)
    allowed_roots = (DOCS_DIR.resolve(strict=False),)
    if not any(
        _is_relative_to(resolved, allowed_root)
        for allowed_root in allowed_roots
    ):
        raise DocsError(
            "文档读写只允许位于 .tmp/kasumi-docs 内"
        )
    if resolved.suffix.lower() not in {".md", ".mdx"}:
        raise DocsError("文档文件必须使用 .md 或 .mdx 扩展名")
    return resolved


def _is_relative_to(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
    except ValueError:
        return False
    return True


def default_doc_path(path: str) -> Path:
    validate_doc_path(path)
    candidate = DOCS_DIR.joinpath(*path.split("/")).with_suffix(".md")
    return ensure_docs_output_path(candidate)


def display_path(path: Path) -> str:
    try:
        return path.resolve(strict=False).relative_to(ROOT).as_posix()
    except ValueError:
        return str(path)


def read_text(path: Path) -> str:
    if not path.is_file():
        raise DocsError(f"文件不存在: {display_path(path)}")
    if path.stat().st_size > MAX_DOC_BYTES:
        raise DocsError(
            f"文件超过 {MAX_DOC_BYTES // (1024 * 1024)} MiB 限制: "
            f"{display_path(path)}"
        )
    try:
        with path.open(encoding="utf-8", newline="") as handle:
            return handle.read()
    except UnicodeDecodeError as error:
        raise DocsError(f"文件不是有效的 UTF-8: {display_path(path)}") from error


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=".update-docs-",
        suffix=".tmp",
        dir=str(path.parent),
    )
    temporary_path = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="") as handle:
            handle.write(text)
        os.replace(temporary_path, path)
    except Exception:
        temporary_path.unlink(missing_ok=True)
        raise


def normalize(text: str) -> str:
    return text.replace("\r\n", "\n")


def denormalize(text: str, use_crlf: bool) -> str:
    return text.replace("\n", "\r\n") if use_crlf else text


def parse_env_file(path: Path) -> dict[str, str]:
    if not path.is_file():
        raise DocsError(f"env 文件不存在: {path}")
    values: dict[str, str] = {}
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except UnicodeDecodeError as error:
        raise DocsError(f"env 文件不是有效的 UTF-8: {path}") from error
    for line_number, raw_line in enumerate(lines, start=1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("export "):
            line = line.removeprefix("export ").strip()
        if "=" not in line:
            raise DocsError(f"env 文件第 {line_number} 行缺少等号: {path}")
        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in {"'", '"'}:
            value = value[1:-1]
        if key:
            values[key] = value
    return values


def validate_base_url(value: str) -> str:
    base_url = value.strip().rstrip("/")
    parsed = urllib.parse.urlsplit(base_url)
    if parsed.scheme not in {"https", "http"}:
        raise DocsError("MIOMOE_AGENT_BASE_URL 必须是 HTTP 或 HTTPS 地址")
    if not parsed.hostname:
        raise DocsError("MIOMOE_AGENT_BASE_URL 缺少主机名")
    if parsed.username or parsed.password:
        raise DocsError("MIOMOE_AGENT_BASE_URL 不能包含用户名或密码")
    try:
        parsed.port
    except ValueError as error:
        raise DocsError("MIOMOE_AGENT_BASE_URL 端口无效") from error
    if parsed.query or parsed.fragment:
        raise DocsError("MIOMOE_AGENT_BASE_URL 不能包含查询参数或片段")
    if parsed.path not in {"", "/"}:
        raise DocsError("MIOMOE_AGENT_BASE_URL 不能包含路径")
    loopback_hosts = {"localhost", "127.0.0.1", "::1"}
    if parsed.scheme == "http" and parsed.hostname not in loopback_hosts:
        raise DocsError("非回环地址必须使用 HTTPS")
    return base_url


def load_environment(explicit_env: str | None) -> Environment:
    values: dict[str, str] = {}
    if explicit_env:
        values.update(parse_env_file(Path(explicit_env)))
    elif (ROOT / ".env").is_file():
        values.update(parse_env_file(ROOT / ".env"))

    for key in ("MIOMOE_AGENT_BASE_URL", "MIOMOE_AGENT_KEY"):
        value = os.environ.get(key)
        if value is not None:
            values[key] = value

    base_url = values.get("MIOMOE_AGENT_BASE_URL", "").strip()
    api_key = values.get("MIOMOE_AGENT_KEY", "").strip()
    if not base_url or not api_key:
        raise DocsError(
            "缺少 MIOMOE_AGENT_BASE_URL 或 MIOMOE_AGENT_KEY，"
            "请设置环境变量或使用 --env 指定文件"
        )
    if "\r" in api_key or "\n" in api_key:
        raise DocsError("MIOMOE_AGENT_KEY 不能包含换行符")
    return Environment(validate_base_url(base_url), api_key)


class Client:
    def __init__(self, environment: Environment):
        self.base_url = environment.base_url
        self.api_key = environment.api_key
        self.opener = urllib.request.build_opener(NoRedirectHandler())

    def _request(
        self,
        endpoint: str,
        *,
        method: str = "GET",
        payload: dict[str, Any] | None = None,
        allow_not_found: bool = False,
    ) -> bytes | None:
        if not endpoint.startswith("/"):
            raise DocsError(f"无效的 Agent API 路径: {endpoint}")
        headers = {"X-API-Key": self.api_key}
        body = None
        if payload is not None:
            body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
            headers["Content-Type"] = "application/json"
        request = urllib.request.Request(
            self.base_url + endpoint,
            data=body,
            method=method,
            headers=headers,
        )
        try:
            with self.opener.open(
                request,
                timeout=REQUEST_TIMEOUT_SECONDS,
            ) as response:
                content = response.read(MAX_RESPONSE_BYTES + 1)
        except urllib.error.HTTPError as error:
            if allow_not_found and error.code == 404:
                return None
            detail = error.read(MAX_RESPONSE_BYTES).decode("utf-8", "replace")
            raise DocsError(
                f"HTTP {error.code} {request.full_url}\n{detail[:600]}"
            ) from error
        except (urllib.error.URLError, TimeoutError, OSError) as error:
            raise DocsError(f"网络请求失败 {request.full_url}: {error}") from error

        if len(content) > MAX_RESPONSE_BYTES:
            raise DocsError(
                f"响应超过 {MAX_RESPONSE_BYTES // (1024 * 1024)} MiB 限制"
            )
        return content

    def _json(
        self,
        endpoint: str,
        *,
        method: str = "GET",
        payload: dict[str, Any] | None = None,
    ) -> dict[str, Any]:
        content = self._request(endpoint, method=method, payload=payload)
        if content is None:
            raise DocsError(f"Agent API 意外返回空响应: {endpoint}")
        try:
            value = json.loads(content.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as error:
            raise DocsError(f"Agent API 返回了无效 JSON: {endpoint}") from error
        if not isinstance(value, dict):
            raise DocsError(f"Agent API 返回结构不是对象: {endpoint}")
        return value

    def me(self) -> dict[str, Any]:
        return self._json("/agent/me").get("data") or {}

    def docs(self) -> dict[str, Any]:
        return self._json("/agent/docs").get("data") or {}

    def raw(self, path: str) -> str:
        endpoint = "/agent/docs/raw?" + urllib.parse.urlencode({"path": path})
        content = self._request(endpoint)
        if content is None:
            raise DocsError(f"在线文档不存在: {path}")
        if len(content) > MAX_DOC_BYTES:
            raise DocsError(
                f"在线文档超过 {MAX_DOC_BYTES // (1024 * 1024)} MiB 限制: {path}"
            )
        try:
            return content.decode("utf-8")
        except UnicodeDecodeError as error:
            raise DocsError(f"在线文档不是有效的 UTF-8: {path}") from error

    def try_raw(self, path: str) -> str | None:
        endpoint = "/agent/docs/raw?" + urllib.parse.urlencode({"path": path})
        content = self._request(endpoint, allow_not_found=True)
        if content is None:
            return None
        if len(content) > MAX_DOC_BYTES:
            raise DocsError(
                f"在线文档超过 {MAX_DOC_BYTES // (1024 * 1024)} MiB 限制: {path}"
            )
        try:
            return content.decode("utf-8")
        except UnicodeDecodeError as error:
            raise DocsError(f"在线文档不是有效的 UTF-8: {path}") from error

    def page(self, path: str) -> dict[str, Any]:
        endpoint = "/agent/docs/page?" + urllib.parse.urlencode({"path": path})
        return self._json(endpoint).get("data") or {}

    def revisions(self, status: str | None, page: int) -> dict[str, Any]:
        query: dict[str, str | int] = {"page": page}
        if status:
            query["status"] = status
        endpoint = "/agent/docs/revisions?" + urllib.parse.urlencode(query)
        return self._json(endpoint).get("data") or {}

    def revision(self, uuid: str) -> dict[str, Any]:
        endpoint = "/agent/docs/revisions/" + urllib.parse.quote(uuid, safe="")
        return self._json(endpoint).get("data") or {}

    def submit(
        self,
        path: str,
        title: str,
        description: str,
        content: str,
        note: str,
        endpoint: str = "/agent/docs/revisions",
    ) -> dict[str, Any]:
        return self._json(
            endpoint,
            method="POST",
            payload={
                "path": path,
                "title": title,
                "description": description,
                "content": content,
                "note": note,
            },
        )


def split_frontmatter(text: str) -> tuple[dict[str, str], str]:
    metadata: dict[str, str] = {}
    body = text
    normalized = normalize(text)
    if normalized.startswith("---\n"):
        end = normalized.find("\n---", 4)
        if end != -1:
            for line in normalized[4:end].splitlines():
                if ":" not in line:
                    continue
                key, value = line.split(":", 1)
                metadata[key.strip()] = value.strip().strip("\"'")
            body = normalized[end + 4 :].lstrip("\n")
    return metadata, body


def show_diff(before: str, after: str, path: str) -> bool:
    lines = list(
        difflib.unified_diff(
            normalize(before).splitlines(True),
            normalize(after).splitlines(True),
            fromfile=f"online:{path}",
            tofile=f"new:{path}",
            n=2,
        )
    )
    if not lines:
        return False
    sys.stdout.write("".join(lines))
    return True


def confirm_submission(path: str, skip_confirmation: bool) -> None:
    if skip_confirmation:
        return
    if not sys.stdin.isatty():
        raise DocsError(
            f"非交互环境必须加 --yes 才能提交: {path}"
        )
    answer = input(f"输入 {path} 确认提交: ").strip()
    if answer != path:
        raise DocsError("确认内容不匹配，已取消提交")


def publish(
    client: Client,
    *,
    path: str,
    content: str,
    note: str,
    description: str | None,
    dry_run: bool,
    yes: bool,
) -> None:
    validate_doc_path(path)
    online = client.raw(path)
    use_crlf = "\r\n" in online
    updated = denormalize(normalize(content), use_crlf)
    if normalize(updated) == normalize(online):
        raise DocsError(f"内容与线上一致，不提交: {path}")
    if not show_diff(online, updated, path):
        raise DocsError(f"归一化后没有差异，不提交: {path}")
    if dry_run:
        print(f"[dry-run] 未提交: {path}")
        return
    if not note.strip():
        raise DocsError("正式提交必须提供 --note")

    confirm_submission(path, yes)
    page = client.page(path)
    title = page.get("title")
    if not isinstance(title, str) or not title.strip():
        raise DocsError(f"线上页面缺少标题，无法提交: {path}")
    online_description = page.get("description")
    resolved_description = (
        description
        if description is not None
        else online_description
        if isinstance(online_description, str)
        else ""
    )
    result = client.submit(
        path,
        title,
        resolved_description,
        updated,
        note,
    )
    data = result.get("data")
    revision = data if isinstance(data, dict) else {}
    print(
        f"已提交 {path} -> {result.get('code')} "
        f"| uuid={revision.get('uuid')} | status={revision.get('status')}"
    )


def create_page(
    client: Client,
    *,
    path: str,
    content: str,
    title: str,
    description: str,
    note: str,
    dry_run: bool,
    yes: bool,
) -> None:
    validate_doc_path(path)
    if client.try_raw(path) is not None:
        raise DocsError(f"页面已存在，改用 push 提交修订: {path}")

    metadata, body = split_frontmatter(content)
    resolved_title = title or metadata.get("title", "")
    resolved_description = description or metadata.get("description", "")
    if not resolved_title:
        raise DocsError("新建页面必须提供 --title 或 frontmatter title")
    if body.startswith("# "):
        body = body.split("\n", 1)[1].lstrip("\n")
    page_content = (
        "---\n"
        f"title: {json.dumps(resolved_title, ensure_ascii=False)}\n"
        f"description: {json.dumps(resolved_description, ensure_ascii=False)}\n"
        "---\n\n"
        f"{normalize(body).rstrip()}\n"
    )
    if dry_run:
        sys.stdout.write(page_content)
        print(f"[dry-run] 未创建: {path}")
        return
    if not note.strip():
        raise DocsError("正式创建页面必须提供 --note")

    confirm_submission(path, yes)
    result = client.submit(
        path,
        resolved_title,
        resolved_description,
        page_content,
        note,
        endpoint="/agent/docs",
    )
    data = result.get("data")
    document = data if isinstance(data, dict) else {}
    print(
        f"已创建 {path} -> {result.get('code')} "
        f"| uuid={document.get('uuid')} | status={document.get('status')}"
    )


def cmd_me(client: Client, _args: argparse.Namespace) -> None:
    data = client.me()
    permissions = data.get("permissions")
    permission_text = (
        ", ".join(str(item) for item in permissions)
        if isinstance(permissions, list)
        else ""
    )
    print(f"账号: {data.get('name')} ({data.get('display_name')})")
    print(f"组:   {data.get('group')}")
    print(f"权限: {permission_text}")


def cmd_list(client: Client, args: argparse.Namespace) -> None:
    data = client.docs()
    metas = data.get("metas")
    pages = data.get("pages")
    if not isinstance(metas, list) or not isinstance(pages, list):
        raise DocsError("Agent API 文档列表结构无效")
    for meta in metas:
        if not isinstance(meta, dict) or meta.get("path") not in {
            "kasumi",
            "skill",
        }:
            continue
        page_count = meta.get("pages")
        count = len(page_count) if isinstance(page_count, list) else 0
        print(f"kasumi  {meta.get('title')}  {count} 页")
    for page in pages:
        if not isinstance(page, dict):
            continue
        path = page.get("path")
        if not isinstance(path, str) or not is_allowed_doc_path(path):
            continue
        if args.paths:
            print(path)
            continue
        print(
            f"{page.get('sort_order')!s:<6} "
            f"{path:<42} {page.get('title')}"
        )


def cmd_show(client: Client, args: argparse.Namespace) -> None:
    path = validate_doc_path(args.path)
    sys.stdout.write(client.raw(path))


def cmd_pull(client: Client, args: argparse.Namespace) -> None:
    path = validate_doc_path(args.path)
    target = default_doc_path(path)
    if args.out:
        target = ensure_docs_output_path(Path(args.out))
    content = client.raw(path)
    write_text(target, content)
    line_ending = "CRLF" if "\r\n" in content else "LF"
    print(
        f"已拉取 {path} -> {display_path(target)} "
        f"({len(content.encode('utf-8'))} 字节，行尾 {line_ending})"
    )


def cmd_push(client: Client, args: argparse.Namespace) -> None:
    source = ensure_docs_output_path(Path(args.file))
    path = validate_doc_path(args.path)
    content = read_text(source)
    publish(
        client,
        path=path,
        content=content,
        note=args.note or "",
        description=args.description,
        dry_run=args.dry_run,
        yes=args.yes,
    )


def cmd_create(client: Client, args: argparse.Namespace) -> None:
    source = ensure_docs_output_path(Path(args.file))
    path = validate_doc_path(args.path)
    content = read_text(source)
    create_page(
        client,
        path=path,
        content=content,
        title=args.title or "",
        description=args.description or "",
        note=args.note or "",
        dry_run=args.dry_run,
        yes=args.yes,
    )


def cmd_status(client: Client, args: argparse.Namespace) -> None:
    if args.uuid:
        data = client.revision(args.uuid)
        path = data.get("path")
        if not isinstance(path, str) or not is_allowed_doc_path(path):
            raise DocsError("该修订不属于受支持的 Kasumi 文档分区")
        for key in (
            "uuid",
            "path",
            "title",
            "status",
            "note",
            "review_note",
            "created_at",
            "reviewed_at",
        ):
            print(f"{key:<12} {data.get(key)}")
        return

    data = client.revisions(args.status, args.page)
    items = data.get("items")
    if not isinstance(items, list):
        raise DocsError("Agent API 修订列表结构无效")
    for item in items:
        if not isinstance(item, dict):
            continue
        path = item.get("path")
        if not isinstance(path, str) or not is_allowed_doc_path(path):
            continue
        print(
            f"{item.get('status')!s:<10} "
            f"{item.get('uuid')!s:<38} "
            f"{item.get('path')!s:<24} "
            f"{item.get('title')}"
        )
    pagination = data.get("pagination")
    if isinstance(pagination, dict):
        print(
            f"第 {pagination.get('current_page')}/"
            f"{pagination.get('last_page')} 页，"
            f"共 {pagination.get('total')} 条"
        )


def positive_int(value: str) -> int:
    try:
        number = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("必须是整数") from error
    if number < 1:
        raise argparse.ArgumentTypeError("必须大于等于 1")
    return number


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Kasumi 在线文档读写工具",
    )
    parser.add_argument("--env", help="显式指定包含 Agent 凭据的 env 文件")
    subparsers = parser.add_subparsers(dest="command", required=True)

    me_parser = subparsers.add_parser("me", help="检查账号与权限")
    me_parser.set_defaults(handler=cmd_me)

    list_parser = subparsers.add_parser("list", help="列出 kasumi 文档")
    list_parser.add_argument("--paths", action="store_true", help="只输出路径")
    list_parser.set_defaults(handler=cmd_list)

    show_parser = subparsers.add_parser("show", help="打印线上原始 MDX")
    show_parser.add_argument("path")
    show_parser.set_defaults(handler=cmd_show)

    pull_parser = subparsers.add_parser("pull", help="拉取文档到本地")
    pull_parser.add_argument("path")
    pull_parser.add_argument(
        "-o",
        "--out",
        help="输出文件，必须位于 .tmp/kasumi-docs 内",
    )
    pull_parser.set_defaults(handler=cmd_pull)

    push_parser = subparsers.add_parser(
        "push",
        help="从本地文件提交文档修订",
    )
    push_parser.add_argument("file")
    push_parser.add_argument("--path", required=True, help="目标文档路径")
    push_parser.add_argument("--note", help="修订说明，正式提交时必填")
    push_parser.add_argument(
        "--description",
        help="同时更新页面描述；缺省保留线上描述",
    )
    push_parser.add_argument(
        "--dry-run",
        action="store_true",
        help="只显示 diff，不提交",
    )
    push_parser.add_argument(
        "--yes",
        action="store_true",
        help="跳过交互确认，仅用于已检查 diff 的自动化流程",
    )
    push_parser.set_defaults(handler=cmd_push)

    create_parser = subparsers.add_parser(
        "create",
        help="从本地文件新建线上文档页面",
    )
    create_parser.add_argument("file")
    create_parser.add_argument("--path", required=True, help="目标文档路径")
    create_parser.add_argument("--title", help="页面标题")
    create_parser.add_argument("--description", help="页面描述")
    create_parser.add_argument("--note", help="创建说明，正式创建时必填")
    create_parser.add_argument(
        "--dry-run",
        action="store_true",
        help="只显示将创建的全文，不提交",
    )
    create_parser.add_argument(
        "--yes",
        action="store_true",
        help="跳过交互确认，仅用于已检查内容的自动化流程",
    )
    create_parser.set_defaults(handler=cmd_create)

    status_parser = subparsers.add_parser(
        "status",
        help="查看修订列表或单条详情",
    )
    status_parser.add_argument("uuid", nargs="?")
    status_parser.add_argument(
        "--status",
        choices=("pending", "approved", "rejected"),
    )
    status_parser.add_argument("--page", type=positive_int, default=1)
    status_parser.set_defaults(handler=cmd_status)

    return parser


def main(argv: Sequence[str] | None = None) -> int:
    configure_stdio()
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        environment = load_environment(args.env)
        client = Client(environment)
        args.handler(client, args)
        return 0
    except DocsError as error:
        print(f"错误: {error}", file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        print("已取消", file=sys.stderr)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
