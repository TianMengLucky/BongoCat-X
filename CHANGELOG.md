# 更新日志

本文件记录 BongoCat-X 分支（基于 [vladelaina/BongoCat](https://github.com/vladelaina/BongoCat)）
相对上游的变更。格式遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)。

发布约定：推送 `v<版本号>` 标签（须与 `CMakeLists.txt` 中的项目版本一致）会自动触发
Release 工作流构建并发布 GitHub Release；发布说明取自本文件，优先匹配 `## [<版本号>]`
小节，若无匹配则使用最上方小节。发版前请把「未发布」小节标题改为对应版本号。

## [未发布] · 2026-10-01

### 新增

- **Windows「仅在录屏软件中显示」模式**：通过 DWM cloaking 将桌宠窗口从桌面隐藏，OBS 等录屏/直播软件仍能采集到画面。来自上游 PR [#71](https://github.com/vladelaina/BongoCat/pull/71)（作者 Fid-falling，分支 `feature/capture-only`）及其吸收的上游 main 改动，同时引入「保持在屏幕内」开关与模型渲染质量等选项。
- **模型隐藏**：模型卡片角标快捷隐藏，配套隐藏模型视图切换（`feat/hide-models`）。
- **动作显示名**：支持从模型配置文件读取动作显示名称（`feat/motion-config-naming`）。
- **随机行为选择器**：随机表情/动作可逐项启用与禁用（`feat/random-behavior-config`）。
- **SDK 缺失提示**：未启用 Live2D 渲染的诊断构建打开设置界面时，右下角弹出 toast（12 秒，悬停暂停倒计时，每次运行一次），指引从 Live2D 官网手动下载 SDK 并放入指定目录后重新构建。
- `AGENTS.md`（构建、CI 门槛与 AI 协作约定）与本 `CHANGELOG.md`。
- **自动发布流水线**：推送 `v*` 标签时自动构建全平台产物（Windows x64/x86 安装与便携版、macOS、Linux、AppImage）并创建 GitHub Release；发布说明自动取自 `CHANGELOG.md` 对应版本小节（无匹配时回退到最上方小节），附全平台下载链接与 SHA-256 校验提示。VirusTotal 扫描仍仅在上游仓库启用。

### 变更

- **应用图标**更换为粉色猫头 + X 标记：同步替换 exe 内嵌 `icon.ico`（16–256px）、macOS `icon.icns`、托盘图标与设置页 Logo（`assets/bongocat.png`）。
- **Live2D Cubism SDK 改为必须手动下载**：SDK 为专有软件不再随构建静默降级——缺失时 CMake 配置直接失败并给出分步导入指引（官网下载、解压布局、GLEW 补充、VS2022 要求）；`BONGO_CAT_REQUIRE_CUBISM` 默认 `ON`，`build.bat` 默认要求 SDK（设 `BONGOCAT_REQUIRE_CUBISM=0` 可退回诊断后端）。
- 默认 `README.md` 改为简体中文；英文版移至 `docs/README.en-US.md`。README 顶部添加 fork 声明与 Live2D 免责说明（本仓库为上游 fork，与 Live2D 公司无关联，不附带 SDK，需用户自行下载导入）；项目状态与贡献者图指向本仓库；移除赞助商与 linux.do 板块及上游 Microsoft Store 徽章。移除 Discord/QQ/微信社区徽章；Live2D 免责说明独立为「⚠️ Live2D 声明」章节；README 头部图标更换为新的 X 猫头图标（`resources/assets/bongocat.png`）。
- **应用内关于页面指向本仓库**：检查更新（Releases API、发布页、产物下载地址）导向本仓库；关于页 Logo、GitHub、贡献者与反馈点击均指向本仓库；移除 Discord/QQ/微信群社区板块、官网链接行及微信二维码相关代码。

### 修复

- `windows_resources.rc` 缺少对 `resources/icons/icon.ico` 的依赖声明，导致增量构建不会重新嵌入更新后的图标。
- 上游 PR #71 使用了未声明的 `BONGO_CAT_PREF_ICON_KEEP_IN_SCREEN` 枚举值（原样合入无法编译）；合并时补全该枚举值并新增对应的"屏幕内窗口"行图标绘制。
