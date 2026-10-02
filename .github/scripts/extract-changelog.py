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
        "## ℹ️ 关于本构建 / About this build\n\n"
        "> 本构建为 **runtime-Core 构建**：内置 Live2D 渲染支持，但**不包含** Live2D "
        "Cubism Core 运行库（专有授权，不随包分发），**并非**无 Live2D 能力的诊断构建。"
        "检测到 Core 后即可渲染 Live2D 模型；未检测到时回退到诊断后端并在设置窗口提示。\n\n"
        "## 🎭 启用 Live2D 教程 / Enable Live2D\n\n"
        "1. **应用内导入（推荐 / Recommended）**：打开「设置 → 模型」页，点击"
        "「导入 Live2D Core」，选择 `Live2DCubismCore.dll` 或官方 Cubism SDK 的 zip "
        "压缩包，导入后立即生效（无需重启）。\n"
        "   Open *Settings → Models* and click *Import Live2D Core*, then pick a "
        "`Live2DCubismCore.dll` or the official Cubism SDK zip. Takes effect "
        "immediately, no restart needed.\n"
        "2. **live2d 文件夹投放 / Drop into the live2d folder**：从 "
        "[Cubism SDK 下载页面](https://www.live2d.com/en/sdk/download/native/)"
        "（需同意 Live2D 许可协议）下载 **Cubism SDK for Native**，将 zip 或解压出的 "
        "`Live2DCubismCore.dll` 放入应用目录/数据目录下的 `live2d` 文件夹，重启应用后"
        "自动识别启用。\n"
        "   Download **Cubism SDK for Native** from the [official download page]"
        "(https://www.live2d.com/en/sdk/download/native/) (accept Live2D's "
        "license), then put the zip or the extracted `Live2DCubismCore.dll` into "
        "the `live2d` folder next to the app or inside the data directory and "
        "restart the app.\n\n"
        "SDK 为 Live2D Inc. 的专有软件，本仓库与其无关联、不分发该 SDK，使用须遵守 "
        "Live2D 的许可协议。The SDK is proprietary software of Live2D Inc.; this "
        "repository is not affiliated with and does not distribute it.\n")

    pathlib.Path(args.output).write_text(body, encoding="utf-8")
    print(f"release notes written from section [{heading}] -> {args.output}")


if __name__ == "__main__":
    main()
