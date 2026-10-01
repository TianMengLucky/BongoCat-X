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
- **注册表开机自启动（Windows）**：自启动改为写入 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`（任务管理器「启动应用」可见），不再使用启动文件夹快捷方式与计划任务，切换开关不再触发 UAC；应用启动时自动迁移并清理旧机制的残留项（旧快捷方式、StartupApproved 记录与旧计划任务）。
- **「以管理员身份运行」开关（Windows，通用设置页）**：原「游戏兼容模式」的提权能力独立为明确选项并移至通用页——开启后应用请求管理员权限并自动重启，此后每次启动（包括开机自启动后的自动提权）都以管理员运行，关闭则从下次启动恢复普通权限；该选项与开机自启动完全解耦（注册表自启动条目不受其影响，始终以普通权限开机拉起，若开启本选项则启动后自行请求提权）。
- **i18n 容错**：语言包文件缺失或损坏时应用回退英文运行，不再导致启动失败；界面文案在当前语言缺键时按键回退英文（原有行为保持）。
- **自动发布流水线**：推送 `v*` 标签时自动构建全平台产物（Windows x64/x86 安装与便携版、macOS、Linux、AppImage）并创建 GitHub Release；发布说明自动取自 `CHANGELOG.md` 对应版本小节（无匹配时回退到最上方小节），附全平台下载链接与 SHA-256 校验提示。VirusTotal 扫描仍仅在上游仓库启用。

### 变更

- **应用图标**更换为粉色猫头 + X 标记：同步替换 exe 内嵌 `icon.ico`（16–256px）、macOS `icon.icns`、托盘图标与设置页 Logo（`assets/bongocat.png`）。
- **Live2D Cubism SDK 改为必须手动下载**：SDK 为专有软件不再随构建静默降级——缺失时 CMake 配置直接失败并给出分步导入指引（官网下载、解压布局、GLEW 补充、VS2022 要求）；`BONGO_CAT_REQUIRE_CUBISM` 默认 `ON`，`build.bat` 默认要求 SDK（设 `BONGOCAT_REQUIRE_CUBISM=0` 可退回诊断后端）。
- 默认 `README.md` 改为简体中文；英文版移至 `docs/README.en-US.md`。README 顶部添加 fork 声明与 Live2D 免责说明（本仓库为上游 fork，与 Live2D 公司无关联，不附带 SDK，需用户自行下载导入）；项目状态与贡献者图指向本仓库；移除赞助商与 linux.do 板块及上游 Microsoft Store 徽章。移除 Discord/QQ/微信社区徽章；Live2D 免责说明独立为「⚠️ Live2D 声明」章节；README 头部图标更换为新的 X 猫头图标（`resources/assets/bongocat.png`）。
- **应用内关于页面指向本仓库**：检查更新（Releases API、发布页、产物下载地址）导向本仓库；关于页 Logo、GitHub、贡献者与反馈点击均指向本仓库；移除 Discord/QQ/微信群社区板块、官网链接行及微信二维码相关代码。

### 修复

- 完成关于页社区板块移除（`aaf6fc7`）的未尽清理：删除 `preferences.c` 中残留的二维码弹窗状态引用、无实现仍被调用的 `bongo_cat_about_overlays`，并为 `preferences_about_community.c` 补回本地 `hit` 助手、改用 `SDL_OpenURL`——自该提交起主干无法编译，现恢复正常。
- `CheckLocalization` 构建门禁改为分级：仅 zh-CN / en-US 缺键报错，其余语言缺键降级为警告（与 i18n 测试策略一致，上游遗留的 `renderQuality` 缺口不再阻塞构建）。
- `bongo_cat_app_state_tests` 链接修复：补上 `bongo_cat_shortcut_equal` 所在的 `sound_shortcut.c`。

- i18n 测试改为分级校验：仅 zh-CN / en-US（基准语言）缺键或键结构不一致时报错，其余语言缺键降级为警告并回退英文。

- `windows_resources.rc` 缺少对 `resources/icons/icon.ico` 的依赖声明，导致增量构建不会重新嵌入更新后的图标。
- 上游 PR #71 使用了未声明的 `BONGO_CAT_PREF_ICON_KEEP_IN_SCREEN` 枚举值（原样合入无法编译）；合并时补全该枚举值并新增对应的"屏幕内窗口"行图标绘制。
