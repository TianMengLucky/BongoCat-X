#!/usr/bin/env python3
"""Compose GitHub Release notes for a tag from CHANGELOG.md.

Finds the "## [<version>]" section matching the tag (the leading "v" of
tags like v1.2.3 is stripped). When the changelog has no entry for the
version yet - e.g. the newest section is still "[未发布]" / "[Unreleased]"
- the top-most section is used instead, so tagging never produces empty
release notes. The composed body is the notes followed by per-platform
download links for this repository.

Usage:
  python3 extract-changelog.py --tag v1.2.3 \
    --repository TianMengLucky/BongoCat-X --output release-notes.md
"""

import argparse
import pathlib
import re

SECTION = re.compile(r"^## \[([^\]]+)\][^\n]*\n", re.MULTILINE)

PLATFORMS = [
    ("windows-x64-setup.exe", "Windows x64 安装版"),
    ("windows-x64-portable.exe", "Windows x64 便携版"),
    ("windows-x86-setup.exe", "Windows x86 安装版"),
    ("windows-x86-portable.exe", "Windows x86 便携版"),
    ("macos-arm64.zip", "macOS Apple Silicon"),
    ("macos-x64.zip", "macOS Intel"),
    ("linux-x64.tar.gz", "Linux x64 (tar.gz)"),
    ("linux-x64.AppImage", "Linux x64 AppImage"),
]


def extract_section(text: str, version: str) -> tuple[str, str]:
    matches = list(SECTION.finditer(text))
    if not matches:
        raise SystemExit("CHANGELOG.md has no '## [<heading>]' sections")
    chosen = next((m for m in matches if m.group(1).strip() == version), matches[0])
    end = SECTION.search(text, chosen.end())
    body = text[chosen.end(): end.start() if end else len(text)]
    return chosen.group(1).strip(), body.strip("\n")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tag", required=True, help="release tag, e.g. v1.2.3")
    parser.add_argument("--changelog", default="CHANGELOG.md")
    parser.add_argument("--repository", required=True, help="owner/repo")
    parser.add_argument("--output", required=True, help="body file for gh-release")
    args = parser.parse_args()

    version = args.tag[1:] if args.tag.startswith("v") else args.tag
    heading, notes = extract_section(
        pathlib.Path(args.changelog).read_text(encoding="utf-8"), version)

    links = "\n".join(
        f"- [{label}](https://github.com/{args.repository}/releases/download/"
        f"{args.tag}/BongoCat-X-{version}-{name})" for name, label in PLATFORMS)

    body = (
        f"{notes}\n\n"
        f"## 📦 下载 / Download ({heading})\n\n"
        f"{links}\n\n"
        "每个产物都附带 `.sha256` 校验文件。\n\n"
        "> 本构建不包含 Live2D Cubism SDK（使用诊断渲染后端）。SDK 为专有授权，"
        "如需启用 Live2D 模型，请按 README 的说明在本地自行构建。\n")

    pathlib.Path(args.output).write_text(body, encoding="utf-8")
    print(f"release notes written from section [{heading}] -> {args.output}")


if __name__ == "__main__":
    main()
