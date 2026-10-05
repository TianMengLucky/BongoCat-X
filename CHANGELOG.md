# 更新日志

本文件记录 BongoCat-X 分支（基于 [vladelaina/BongoCat](https://github.com/vladelaina/BongoCat)）
相对上游的变更。格式遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)。

发布约定：推送 `v<版本号>` 标签（须与 `CMakeLists.txt` 中的项目版本一致）会自动触发
Release 工作流构建并发布 GitHub Release；发布说明取自本文件，优先匹配 `## [<版本号>]`
小节，若无匹配则使用最上方小节。发版前请把「未发布」小节标题改为对应版本号。

## [未发布]

### 新增

- **强制接收鼠标输入**（Windows）：「模型设置」新增开关，开启后模型指针不再等待光标锁定检测器判定，直接采信 Raw Input 设备移动量驱动（`settings` 新键 `rendering.forceMouseInput`）。修复部分游戏隐藏并钉住系统光标（每帧回中带抖动、稍大的 ClipCursor 区域、低回报率鼠标等命不中检测阈值）时桌宠无法跟随鼠标移动的问题；锁定检测仍照常运行，选项关闭时行为不变。

## [2.0.4] · 2026-10-05

### 新增

- **内存安全关键的解析器迁移到 Rust**：新增 `src/rust/bongo-safe` crate（通过 Corrosion 以静态库链接进应用，C ABI 见 `include/bongo_cat/safe_ffi.h`），所有不可信字节流（网络响应、用户导入的模型文件）改由安全 Rust 解析：
  - **SHA-256**：删除 `src/core/sha256.c` 中的手写实现，改用 `sha2` crate；`bongo_cat_sha256_*` 公共 API 与取消语义不变。
  - **图像解码**：便携解码路径（模型贴图等）与贡献者头像的 PNG/JPEG/WebP 解码改用 `image` crate，带硬性尺寸/内存上限；移除 stb_image 解码实现与 libwebp 依赖（`cmake/AboutWebP.cmake` 删除）。Windows 的 WIC 系统解码路径、stb_image_write/resize2（编码与缩放，不解析不可信输入）保留。
  - **贡献者信息流**：「关于」页 SVG 解析与头像（贡献者可控的网络数据）解码/缩放/圆形蒙版整体迁移到 Rust（`roxmltree` + `image`）；删除 `preferences_about_svg.c` 与 vendored nanosvg。
  - **音频解码**：用户音效文件改由 `symphonia` crate 解码为 PCM，miniaudio 继续负责输出设备（`ma_sound` 现基于零拷贝 `ma_audio_buffer`）；解码量有硬性上限（约 5 分钟 48 kHz 立体声），取代原先的流式解码。
  - **表情文件**：`.exp3.json` 解析迁移到 Rust（`serde_json`），桥接层直接以解析结果构造 `CubismExpressionMotion`，不再把表情字节交给 Cubism SDK 自带的 JSON 解析器。
  - **配置 JSON**：`bongocat/settings` 与 `bongocat/session` 的解析与序列化迁移到 `bongo-safe`（`config.rs` + `unique.rs`，serde_json）。C 侧保留文件读取、原子写入（临时文件 + flush + replace + 目录 fsync）与 `settings_validate` 校验；字段级容忍语义全部复刻：缺省键保留默认值、类型错误报格式错、重复键拒绝（自定义 `UniqueValue` 反序列化器）、旧键迁移（`randomModel`→`sequentialModel`、`gameCompatibility`→`runAsAdmin`、`#rrggbb` 颜色）、扩展 JSON 原样透传。`config_fields.c`、`config_session_io.c`、`config_settings_save.c` 删除，`config_io.c` 改为薄 FFI 封装；FFI 入口校验 `sizeof` 防止镜像结构与 `config.h` 漂移。
- **FFI 测试**：新增 `tests/core/test_safe_ffi.c`（CTest `safe-ffi`）与 crate 内 18 个单元测试（`cargo test --manifest-path src/rust/bongo-safe/Cargo.toml`）。

### 修复

- **语言列表字形缺失**：设置页语言切换列表的原生语言名自 WIP 起硬编码进 UI 代码，字体图集字形范围不再覆盖（「简」等字符渲染为方块）。语言名下沉为 `bongo_cat_ui_language_name`（i18n 核心单一来源，`preferences_pages.c` 改为引用），全字形范围收集优先纳入这些字符；i18n 测试的全范围容量放大（2048 → 8192 个数字，即约 1023 → 4095 个 range，避免韩文等高位码点被 `build_ranges` 静默截断）。
- **更新检查测试夹具过期**：`test_update.c` 的发布资产 URL 仍指向上游 `vladelaina/BongoCat`，而 `update_release.c` 的信任前缀已改为 fork 仓库，导致解析一律失败；同步更新夹具 URL。
- **frame-policy 测试断言过严**：余量花费步骤会把帧精确推到面积预算上，浮点边距令 `<= 2.0` 严格比较系统性失败；放宽为 `2.0 + 1e-4` 并注释原因。
- **虚拟手柄测试环境适配**：宿主拒绝虚拟手柄时以 skip 返回码 77 优雅跳过，不再硬性失败。
- **移除无法编译的僵尸测试**：`preferences-lifecycle` / `model-startup-recovery` 引用的 `qr_*` 字段随社区卡片功能移除（aaf6fc7）而消失，测试自那时起无法编译；连同 `CheckModelStartupRecovery.cmake` 一并移除。
- **测试目标源列表补齐**：`hover-fade` 测试补上 `modal_frame.c` 新增调用所依赖的 `resource_trace.c` 与 `platform/common/memory.c` 并为新增调用提供测试桩；`gl_readback.c` / `window_frame.c` 修复 /W4 变量遮蔽（C4459）使相关测试目标恢复编译。

### 变更

- **CI 统一使用 sccache**：quality / sanitizers / build 各 job 的 C/C++（`CMAKE_*_COMPILER_LAUNCHER`）与 Rust（`RUSTC_WRAPPER`）编译统一经 sccache 缓存；build/release 矩阵增加 cargo registry 缓存，并在所有配置 CMake 的 job 显式固定 stable 工具链（minimal profile）。
- **构建现在需要 Rust 工具链**（cargo，CI runner 自带；本地从 rustup 安装）：Corrosion v0.5.2 经 `FetchContent` 获取并在配置阶段构建 `bongo-safe`。全部 10 份 README、AGENTS.md 已同步更新构建说明。

### 修复

- **修复含科学计数法数字的表情文件被静默丢弃**：Cubism SDK 自带的 JSON 解析器（`CubismJson::ParseNumeric`）只接受 `-`、数字与小数点，数字带科学计数法（Cubism Editor 导出的预设即如此，如 `-2.980232238769531e-7`）时整个文件解析失败，表情虽在菜单中可见却永远无法生效（上游 [issue #68](https://github.com/vladelaina/BongoCat/issues/68)）。`.exp3.json` 现由 `bongo-safe` Rust crate 解析，桥接层子类化 `CubismExpressionMotion` 直接填充淡入淡出时间与参数，完整保留 SDK 语义（缺省淡入淡出 1 秒、未知混合模式回退 Add）。

## [2.0.3] · 2026-10-02

### 新增

- **便携存储模式**：设置与数据可存放在可执行文件旁，通过 `BongoCat.ini` 中 `portable = 1` 启用（可手动编辑或从通用设置页切换）；启用时将当前设置复制到 `<exe>/config`，并把已导入的 Cubism Core 一并迁移到 `<exe>/data/live2d`，使 Live2D 运行时放入在便携模式下仍生效。10 个语言包同步新增对应文案。
- **内置模型恢复**：用户删除的内置模型可从应用内嵌资源恢复；模型页新增恢复入口，模型目录运行时层补充资源恢复逻辑。
- **拖动时窗口吸附屏幕边缘**：在「保持在屏幕内」旁新增「边缘吸附」开关；启用后拖动时窗口边缘在距屏幕边缘 20px 内自动磁吸贴合，拖动激活时刷新显示边界确保吸附数据可用。
- **随机模型切换**：新增猫页开关与切换间隔（默认 15 分钟），运行时定时器在参与模型间随机切换（跳过当前模型），参与状态持久化在随机禁用列表中；FPS 范围放宽至 1–360 并支持自由数值输入，渲染质量滑条以 0.1 步进、值稳定 300ms 后才重载（吸附到最近有效步进而非重置）。

### 修复

- **修复贡献者列表构建时的全局缓冲区越界读（ASan 报错）**：`preferences_about_online.c` 拼接 SVG 头时长度硬编码为 96，而字符串实际为 84 字节，多读 12 字节；改为以 `sizeof - 1` 取真实长度。
- **修复 Linux 严格 C11 下 `strcasecmp` 未声明的编译错误**：`portable_mode.c` 中 `SDL_strcasecmp` 在非 Windows 平台展开为 `strcasecmp`，需包含 `<strings.h>`（与 `model_import_path.c` 等既有做法一致）。
- **修复 Release 工作流 `skip-check` 步骤在 GitHub Release 不存在时崩溃**：`gh api` 返回 404 时，`try/catch` 不捕获本机命令非零退出码，导致 `$release` 为 null、`$assets` 含 null 元素，`Where-Object` 调用 `.StartsWith()` 报空引用错误（"You cannot call a method on a null-valued expression"）；改用 `gh release view --json assets` 并以 `$LASTEXITCODE` 判断，加 `$ErrorActionPreference = 'Continue'`、`exit 0` 显式退出码与 null 防护。
- **修复 `skip-check` 步骤以 exit code 1 退出**：`gh api` 的 stderr 经 `2>&1` 重定向后，PowerShell 将其包装为 `ErrorRecord` 混入管道，即使走 else 分支仍使脚本非零退出；改为 `2>$null` 丢弃 stderr 文本，`$GITHUB_OUTPUT` 写入从 `Out-File` 改为 `Add-Content` 避免 UTF-8 BOM 污染输出值。
- **修复 GitHub Actions 警告**：`actions/cache@v4` 与 `softprops/action-gh-release@v2` 使用已废弃的 Node.js 20 运行时，分别升级到 `@v5` 与 `@v3`；`action-gh-release` 的 `allow_updates` 输入在 v3 中已改名为 `overwrite_files`，同步更名。

### 变更

- **发布说明与 README 明确 runtime-Core 构建**：GitHub Release 说明脚注不再误标为「诊断渲染后端」，改为说明官方产物为 runtime-Core 构建（内置 Live2D 渲染、不附带 Core 运行库，未检出时回退诊断后端），并新增「启用 Live2D 教程」（应用内导入 / `live2d` 文件夹投放 + 官方 Cubism SDK 下载页链接）；根 `README.md` 与全部 10 个语言版本的「下载」章节同步补充 runtime-Core 说明与 SDK 下载教程。
- **同步所有 README 语言版本的项目状态章节**：根 `README.md` 与全部 9 个翻译版本的「项目状态」章节统一更新。
- **CI 触发条件限定为 X 分支**：`ci.yml` 的 push 触发条件从 `branches: ["**"]`（所有分支）改为仅 `branches: ["X"]`，`build` job 的分支条件从 `refs/heads/test` 改为 `refs/heads/X`；PR 与手动触发不受影响。

### 开发者

- **构建脚本增强**：`build-windows.ps1` 通过 vswhere 自动检测 Visual Studio 生成器（支持 VS 18 2026，不再仅限 17 2022）；`.gitignore` 增加本地杂散文件（`.zcode` / `.zcodeignore` / `n`）的忽略规则；`AGENTS.md` 补充受保护 `X` 分支说明（变更须通过 `dev` 或功能分支的 PR 合入）。
- **默认构建并行度改为 CPU 核心数**：`build.bat` 与 `build-windows.ps1` 默认并行度从固定值改为自动检测 CPU 核心数。

## [2.0.2] · 2026-10-02

### 修复

- **修复 Linux 构建失败**：严格 C11（无扩展）下 glibc 隐藏 POSIX 扩展，`posix_live2d_sdk.c` 使用 `d_type` / `DT_DIR` 报未声明；在包含头文件前定义 `_DEFAULT_SOURCE` 解决。
- **修复 Windows x86 构建失败**：runtime-Core 构建链接 Core DLL 导入库时未定义 `CSM_CORE_WIN32_DLL`，头文件按 cdecl 声明而 x86 导入库导出 stdcall 装饰名（`_csmXxx@N`），链接出现 52 个未解析外部符号（x64 只有一种调用约定不受影响）；现于 `cmake/Cubism.cmake` 中在 Windows runtime-Core 构建下全局定义 `CSM_CORE_WIN32_DLL=1`。
- **修复 Linux 冒烟测试加载 Core 时崩溃（SIGSEGV）**：`csmGetVersion()` 返回数值版本号（`csmVersion`，`unsigned int`），运行时加载器误按字符串指针解引用传给 `%s`，崩溃在 printf 内部；改为按整数打印 `0x%x`。

### 变更

- **CI 构建缓存统一改用 sccache**：`ci.yml` 与 `release.yml` 的三平台构建作业安装 sccache（Linux apt / macOS brew / Windows choco），经 `actions/cache` 按平台缓存编译产物；Windows 侧通过 `build-windows.ps1` 检测 PATH 中的 sccache 决定是否启用，本地构建不受影响。

## [2.0.1] · 2026-10-02

### 修复

- **修复连续点击「检测 Live2D Core」按钮导致的闪退（堆损坏）**：SDL3 中 `SDL_GetBasePath()` 返回内部持有的缓存指针，重复释放会在第二次重扫时触发 double free（0xC0000374）；相关代码已在 Windows 与 POSIX 平台移除该释放（SDL2 → SDL3 所有权变更）。
- **随机行为选择器弹窗关闭不再弹出**：右键菜单弹窗增加关闭淡出动画，应用退出时跳过以避免残影。
- 修复模型选择进度条与设置窗口淡入的相互干扰。

### 变更

- **Live2D Cubism SDK 改为编译期可选**：`BONGO_CAT_REQUIRE_CUBISM` 默认值由 `ON` 改为 `OFF`——未放置 SDK 时 CMake 配置与编译照常完成，生成不含 Live2D 渲染的诊断后端。由于 Live2D 渲染器必须编译进二进制，该后端**无法**通过运行时放入 SDK 获得渲染能力；只有使用 SDK 构建的版本会响应 `live2d/` 目录（应用旁或数据目录内）的运行时放入，启动时自动检出、重启后生效，也可在设置界面直接导入。SDK 就位时按原方式配置即可得到 Live2D 版本；`ON` 现在仅用于 SDK 缺失时快速失败（release CI 显式传入）。同步调整 `build.bat`（默认不再要求 SDK，设 `BONGOCAT_REQUIRE_CUBISM=1` 恢复）、`release.yml` Unix 配置步骤（显式 `-DBONGO_CAT_REQUIRE_CUBISM=ON`）、诊断包内 `DiagnosticBuildNotice.txt`、`README.md` 与全部 10 个语言的文档。
- **SDK/Core 缺失提示文案调整**：启动日志与设置界面提示按构建类型分别改写——诊断构建明确说明「运行时放入无法在本构建中启用 Live2D，请改用带 Live2D 支持的构建」；运行时 Core 缺失则说明在数据目录的 `live2d` 文件夹放入 `Live2DCubismCore.dll` 或官方 SDK zip（重启自动生效）或在设置窗口导入。10 个语言包的 `live2dSdkMissing` / `live2dCoreMissing` 同步更新。
- **Live2D Core 发现即入库**：无论启动自动检测还是手动点击检测按钮，只要发现 Core（exe 旁裸 DLL、`live2d/` 目录裸 DLL 或官方 SDK zip 解压产物），都会复制一份到数据目录 `data/live2d/Live2DCubismCore.dll` 并把注册表缓存指向该副本；之后即使删除原始 zip/解压目录，重启也能直接从副本加载。
- **检测按钮改为后台扫描**：解压 SDK zip 等耗时操作移入工作线程，点击后立即弹出「正在扫描…」提示，UI 不再卡顿；扫描中重复点击会被忽略，完成后在主线程完成 GL 热切换并弹出结果提示。Core 加载成功时立即清除过时的「缺少 Core」提示。
- **关于页贡献者列表指向本仓库**：从上游站点切换为本仓库 GitHub 贡献者 API，逐人下载头像并本地组装缓存（24 小时）。
- SDL 对话框组件恢复启用（Core 导入的文件选择器依赖）。

### 开发者

- `AGENTS.md` 新增应用日志系统（SDL3 sink 与 INFO 过滤规则）与 SDL3 内存所有权（`SDL_GetBasePath()` 禁止释放）说明。

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
