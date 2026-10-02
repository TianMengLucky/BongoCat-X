# 更新日志

本文件记录 BongoCat-X 分支（基于 [vladelaina/BongoCat](https://github.com/vladelaina/BongoCat)）
相对上游的变更。格式遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)。

发布约定：推送 `v<版本号>` 标签（须与 `CMakeLists.txt` 中的项目版本一致）会自动触发
Release 工作流构建并发布 GitHub Release；发布说明取自本文件，优先匹配 `## [<版本号>]`
小节，若无匹配则使用最上方小节。发版前请把「未发布」小节标题改为对应版本号。

## [未发布]

### 变更

- **Live2D 渲染器改为独立的 `bongo-cat-live2d-backend` 共享库**：Cubism 桥接与渲染实现从可执行文件中拆出，编译为独立的 MODULE 共享库（Linux `libbongo-cat-live2d-backend.so`、macOS `libbongo-cat-live2d-backend.dylib`、Windows `bongo-cat-live2d-backend.dll`）。可执行文件只通过 ABI 分发表（`include/bongo_cat/live2d_backend.h`、`src/live2d/live2d_dispatch.c`）调用渲染能力；启动时 `src/live2d/live2d_backend_load.c` 按「可执行文件旁 → 旁侧 `live2d/` 目录 → 数据目录 `live2d/`」查找该库并加载，命中即启用真实渲染，缺失则继续走 `src/live2d/live2d_stub.c` 诊断后端。库与应用通过主机服务表交互（错误、文件、GL、图片/纹理任务、JSON、sha256、内存与资源统计等）与 10 个 SDL 服务转发（日志、GL 上下文、程序路径、线程、计时），因此库自身不链接 SDL，也能与应用共享同一个 GL 上下文与日志。可执行文件不再链接 Cubism SDK，也不再为 Core 做延迟加载；Core 运行库仍只存在于 `live2d/` 投放或设置界面导入。各打包流程（ZIP/TGZ、Windows 便携版与安装包、AppImage、macOS App Store、微软商店）随附该库；本地 SDK 构建会在构建后把库复制到可执行文件旁。
- **Live2D Cubism SDK 改为编译期可选**：`BONGO_CAT_REQUIRE_CUBISM` 默认值由 `ON` 改为 `OFF`——未放置 SDK 时 CMake 配置与编译照常完成，只生成不含 Live2D 渲染的诊断版可执行文件（**不再**生成渲染库）；该可执行文件仍会在启动时查找 `bongo-cat-live2d-backend`，把同版本的渲染库（如官方 Release 自带的那份）放入应用旁或 `live2d/` 目录即可启用 Live2D 渲染。SDK 就位时按原方式配置即随构建产出该库，启动时自动检出、重启后生效，也可在设置界面直接导入 Core。`ON` 现在仅用于 SDK 缺失时快速失败（release CI 显式传入）。同步调整 `build.bat`（默认不再要求 SDK，设 `BONGOCAT_REQUIRE_CUBISM=1` 恢复）、`release.yml` Unix 配置步骤（显式 `-DBONGO_CAT_REQUIRE_CUBISM=ON`）、诊断包内 `DiagnosticBuildNotice.txt`、`README.md` 与全部 10 个语言的文档。
- **SDK/渲染库/Core 缺失提示文案调整**：启动日志与设置界面提示改为按实际情况分别说明——渲染库缺失时说明把 `bongo-cat-live2d-backend` 放到应用旁或 `live2d` 文件夹（重启后自动启用），或改用自带渲染库的版本；Core 缺失时说明在 `live2d` 文件夹放入 `Live2DCubismCore.dll` 或官方 SDK zip（重启自动生效）或在设置窗口导入。10 个语言包的 `live2dSdkMissing` / `live2dCoreMissing` 同步更新。
- **`gl_readback.c` 局部变量与测试宏重名修复**：`src/platform/common/gl_readback.c` 的 `GLenum error` 与 `tests/platform/test_gl_readback.c` 的 `#define glGetError error` 冲突导致启用测试的构建失败（CI 四个 job 均传 `-DBUILD_TESTING=OFF`，故此前未暴露），局部变量更名为 `gl_error`。

## [2.0.0] · 2026-10-01

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
- **运行时导入 Live2D Cubism Core（Windows）**：启动时自动发现用户提供的 Core（注册表缓存路径 → exe 目录裸 DLL → `live2d/` 目录下的官方 SDK zip）；设置 → 模型页新增「导入 Live2D Core」入口，可直接选择裸 `Live2DCubismCore.dll` 或官方「Cubism SDK for Native」zip 压缩包，导入后**无需重启**即热切换到 Live2D 渲染并重载当前模型；导入位置持久化（HKCU 注册表缓存），重启后仍生效。

### 变更

- **应用图标**更换为粉色猫头 + X 标记：同步替换 exe 内嵌 `icon.ico`（16–256px）、macOS `icon.icns`、托盘图标与设置页 Logo（`assets/bongocat.png`）。
- **Live2D Cubism SDK 改为必须手动下载**：SDK 为专有软件不再随构建静默降级——缺失时 CMake 配置直接失败并给出分步导入指引（官网下载、解压布局、GLEW 补充、VS2022 要求）；`BONGO_CAT_REQUIRE_CUBISM` 默认 `ON`，`build.bat` 默认要求 SDK（设 `BONGOCAT_REQUIRE_CUBISM=0` 可退回诊断后端）。
- 默认 `README.md` 改为简体中文；英文版移至 `docs/README.en-US.md`。README 顶部添加 fork 声明与 Live2D 免责说明（本仓库为上游 fork，与 Live2D 公司无关联，不附带 SDK，需用户自行下载导入）；项目状态与贡献者图指向本仓库；移除赞助商与 linux.do 板块及上游 Microsoft Store 徽章。移除 Discord/QQ/微信社区徽章；Live2D 免责说明独立为「⚠️ Live2D 声明」章节；README 头部图标更换为新的 X 猫头图标（`resources/assets/bongocat.png`）。
- **应用内关于页面指向本仓库**：检查更新（Releases API、发布页、产物下载地址）导向本仓库；关于页 Logo、GitHub、贡献者与反馈点击均指向本仓库；移除 Discord/QQ/微信群社区板块、官网链接行及微信二维码相关代码。
- **CI 构建与发布策略调整**：普通推送仅触发检查，仅当主分支合并进 `test` 分支（或手动触发）时构建并上传各平台构件；CI 与 GitHub Release 均改为构建**不含 Cubism SDK** 的诊断渲染版本（自带 SDK 的版本须用户按 README 在本地构建），发布说明注明诊断后端；发布守卫脚本重写为「禁止除 `mac-app-store.yml` 外的任何工作流恢复或构建 Cubism SDK」，并保留发布类 job 的上游仓库门禁检查。
- **关于页「更多作品」区块移除**：删除区块代码与 `catime.png`、`vlaina.jpg` 资源及其校验逻辑；设置窗口左上角标题图标点击改为指向本仓库。
- **文档**：全部语言 README 的 Live2D / Cubism SDK 章节补充「运行时导入 Live2D Core」提示；移除空置的日文版（`docs/README.ja-JP.md`）及其语言选择器链接。

### 修复

- 完成关于页社区板块移除（`aaf6fc7`）的未尽清理：删除 `preferences.c` 中残留的二维码弹窗状态引用、无实现仍被调用的 `bongo_cat_about_overlays`，并为 `preferences_about_community.c` 补回本地 `hit` 助手、改用 `SDL_OpenURL`——自该提交起主干无法编译，现恢复正常。
- `CheckLocalization` 构建门禁改为分级：仅 zh-CN / en-US 缺键报错，其余语言缺键降级为警告（与 i18n 测试策略一致，上游遗留的 `renderQuality` 缺口不再阻塞构建）。
- `bongo_cat_app_state_tests` 链接修复：补上 `bongo_cat_shortcut_equal` 所在的 `sound_shortcut.c`。

- i18n 测试改为分级校验：仅 zh-CN / en-US（基准语言）缺键或键结构不一致时报错，其余语言缺键降级为警告并回退英文。

- `windows_resources.rc` 缺少对 `resources/icons/icon.ico` 的依赖声明，导致增量构建不会重新嵌入更新后的图标。
- 上游 PR #71 使用了未声明的 `BONGO_CAT_PREF_ICON_KEEP_IN_SCREEN` 枚举值（原样合入无法编译）；合并时补全该枚举值并新增对应的"屏幕内窗口"行图标绘制。
- **拖动卡死（Windows）**：拖动桌宠时若鼠标捕获失败（如输入法抢焦点），指针甩出窗口后松手会导致拖动无法结束、姿势冻结；主循环为左键拖动补上与右键缩放相同的释放恢复兜底（全局左键已释放且无待处理事件时强制结束拖动）。
