<div align="center">
  <a href="https://bongocat.pet" target="_blank">
    <img src="resources/assets/bongocat.png" alt="BongoCat" width="249">
  </a>
  <h1><a href="https://bongocat.pet" target="_blank">BongoCat</a></h1>
</div>

<p align="center">💘 C/C++ × Rust × SDL3，搅拌在一起，尽情敲击！Bong~ Bongo Cat!!!</p>
<p align="center">
  <a href="docs/README.en-US.md">English</a> • <strong>简体中文</strong> • <a href="docs/README.zh-Hant.md">繁體中文</a> • <a href="docs/README.fr-FR.md">Français</a> • <a href="docs/README.de-DE.md">Deutsch</a> • <a href="docs/README.ko-KR.md">한국어</a> • <a href="docs/README.pt-BR.md">Português</a> • <a href="docs/README.ru-RU.md">Русский</a> • <a href="docs/README.es-ES.md">Español</a> • <a href="docs/README.id-ID.md">Bahasa Indonesia</a>
</p>

> [!NOTE]
> 本仓库（**BongoCat-X**）是 [vladelaina/BongoCat](https://github.com/vladelaina/BongoCat) 的 fork 分支。本仓库与 Live2D 公司无关联、不附带 Live2D Cubism SDK，详见下方「Live2D 声明」章节。

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/AGPL--3.0-1fa669?style=flat&logo=gnu&logoColor=white"></a>
  <a href="https://github.com/TianMengLucky/BongoCat-X"><img src="https://img.shields.io/badge/C-A8B9CC?style=flat&logo=c&logoColor=white"></a>

</p>

<div align="center"><video src="https://github.com/user-attachments/assets/75719230-9e49-4124-ae5a-8e35592c5d49" autoplay loop style="border-radius: 8px; max-width: 800px;"></video></div>

> [!TIP]
> 演示中使用的模型来自 [宇痕冫](https://space.bilibili.com/348616056)。
>
> 🎁 想找**免费**模型？我们与才华横溢的模型创作者合作，为您带来丰富多样的免费模型，同时持续探索更多有趣的桌面体验！欢迎访问我们的官方网站：[bongocat.pet](https://bongocat.pet/models)

<p align="center">
  <a href="https://bongocat.pet/models">
    <img height="1080" src="https://github.com/user-attachments/assets/dedd83ca-742a-4f8c-a64a-69e659fca564" />
  </a>
</p>

## 💬 QQ 交流群

使用 QQ 扫描下方二维码，或搜索群号 **211957388**，加入 **BongoCat-X** 交流群。

<p align="center"><img src="resources/assets/qq-group.jpg" alt="BongoCat-X QQ · 211957388" width="320"></p>

## Live2D SDK 与颜色设置

SDK 的导入与下载统一位于「设置 → 插件」，模型页仅管理模型。Windows 用户启用 Live2D 插件后，可点击「下载 Live2D SDK」，阅读并同意两份官方协议后自动下载并安装匹配的 Core；下载显示已接收量、总量与百分比，失败可在弹窗内重试。其他平台请手动导入兼容的 Core。外观与纯色背景共用点击色块打开的调色板弹窗。

## 📥 下载

- GitHub Releases ([v2.2.2](https://github.com/TianMengLucky/BongoCat-X/releases/tag/v2.2.2))

  从 [GitHub Releases](https://github.com/TianMengLucky/BongoCat-X/releases/latest) 下载最新版本。

> [!IMPORTANT]
> 官方 Release 为 **runtime-Core 构建**：内置 Live2D 渲染支持，但**不附带** Live2D Cubism Core 运行库（专有授权，不随包分发），**并非**无 Live2D 能力的诊断构建。未检测到 Core 时会回退到诊断后端并在设置窗口提示。

## ⚠️ Live2D 声明

本仓库与 Live2D 公司（Live2D Inc.）及其官方项目无任何关联。Live2D Cubism SDK 为 Live2D Inc. 的专有软件：本仓库不附带、不内置、也不分发该 SDK。构建前需按下方「Live2D / Cubism SDK」章节的说明，从 Live2D 官网自行下载并导入，使用须遵守 Live2D 的许可协议。

## 🛠️ 从源码构建

BongoCat 使用 CMake 3.24 或更高版本，需要 C11／C++17 编译器、桌面 OpenGL 开发文件和 Rust 工具链（cargo，可通过 rustup 安装）。Corrosion 负责构建 Rust 模块；配置阶段默认下载 SDL3、stb、miniaudio、Nuklear 和 Corrosion，首次构建需要网络连接。

请在项目根目录（包含 `CMakeLists.txt` 的目录）运行以下命令。

### 📋 平台前置条件

- **Windows：** Visual Studio 2022（安装“使用 C++ 的桌面开发”工作负载）和 CMake。请使用 MSVC 生成器；MinGW 可构建诊断后端，但不支持 Cubism SDK。
- **macOS：** Xcode Command Line Tools、CMake 和 Ninja。如果目标架构与主机默认架构不同，请通过 `CMAKE_OSX_ARCHITECTURES` 指定。
- **Linux（Debian/Ubuntu）：** GCC 或 Clang、Ninja，以及 OpenGL/X11 头文件：

  ```bash
  sudo apt-get update
  sudo apt-get install -y build-essential cmake ninja-build \
    libgl1-mesa-dev libx11-dev libxi-dev libxfixes-dev libxcb-screensaver0-dev libfontconfig1-dev fonts-noto-cjk
  ```

### 🔧 配置与构建

在 Linux 和 macOS 上，请使用 Ninja 这样的单配置生成器：

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBONGO_CAT_FETCH_DEPS=ON
cmake --build build --parallel
```

在 Windows 上，请从 Visual Studio 2022 开发者命令行（或 MSVC 可用的其他命令行）运行：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DBONGO_CAT_FETCH_DEPS=ON
cmake --build build --config Release --parallel
```

可执行文件位于：Linux 的 `build/BongoCat`，macOS 的 `build/BongoCat.app/Contents/MacOS/BongoCat`，Visual Studio 构建的 Windows 版本为 `build/Release/BongoCat.exe`。

### 🧪 测试

测试目标默认关闭，以减少日常构建开销。需要测试时，先启用并构建：

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

对于 Visual Studio 这类多配置生成器，请明确指定构建配置：

```powershell
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

### 🎭 Live2D / Cubism SDK（可选：不安装也能编译）

Cubism SDK 仍是可选的专有构建依赖。没有 SDK 时仍可构建宿主和 Inox2D 插件；安装兼容的 Live2D 动态插件并提供 Cubism Core 后即可启用 Live2D，无需重新编译宿主。有 SDK 时会生成独立的 C++ 插件。

若要构建带 Live2D 渲染支持的版本，仍需手动下载并导入 SDK：

1. 打开 [Cubism SDK 下载页面](https://www.live2d.com/en/sdk/download/native/)，同意 Live2D 专有软件许可协议，下载 **Cubism SDK for Native**（项目按 `5-r.5` 版本构建和测试）。
2. 解压压缩包。若解压出的文件夹名为 `CubismSdkForNative-5-r.5`，请将其重命名为 `CubismSdkForNative` 并放到 `vendor/` 目录下，使目录树包含 `Core/` 和 `Framework/`。
3. 较新的 SDK 压缩包不再自带 GLEW。请下载 [GLEW 2.2.0](https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.zip) 并解压到 `vendor/CubismSdkForNative/Samples/OpenGL/thirdParty/glew`（该目录下应直接包含 `include/GL/glew.h` 和 `src/glew.c`）。

也可以将 SDK 保存在任意位置，并通过参数显式指定路径：

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBONGO_CAT_CUBISM_SDK=/path/to/CubismSdkForNative
```

SDK 必须包含 Core 库、Framework 源码，以及 `cmake/Cubism.cmake` 所要求布局中的 OpenGL GLEW 第三方目录。Windows Cubism 构建需要 Visual Studio 2022。SDK 就位后照常配置即可得到带 Live2D 渲染的版本；`BONGO_CAT_REQUIRE_CUBISM=ON` 的作用是让 SDK 缺失时配置直接失败并打印导入方法（发布流水线使用），没有必要时保持默认的 `OFF` 即可。

### ⚙️ CMake 选项

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `BONGO_CAT_FETCH_DEPS` | `ON` | 使用 CMake `FetchContent` 下载固定版本的第三方依赖（含 Corrosion 与 Rust crate 依赖）。仅当 SDL3、stb、miniaudio、Nuklear 和 Corrosion 已可供 CMake 使用时才设为 `OFF`。 |
| `BONGO_CAT_CUBISM_SDK` | `vendor/CubismSdkForNative` | Cubism SDK for Native 的路径。 |
| `BONGO_CAT_REQUIRE_CUBISM` | `OFF` | SDK 缺失时是否让配置失败。默认 `OFF`：缺失时构建不带 Live2D 渲染的诊断后端；设为 `ON` 则要求 SDK 必须存在（发布 CI 使用）。 |
| `BONGO_CAT_WARNINGS_AS_ERRORS` | `OFF` | 将本地编译器警告视为错误。 |

离线构建时将 `BONGO_CAT_FETCH_DEPS=OFF`，并提供 SDL3（包括 `SDL3-static`） 的 CMake 包配置；如果 stb、Nuklear 和 miniaudio 无法自动发现，还需提供其包含目录：

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBONGO_CAT_FETCH_DEPS=OFF \
  -DBONGO_CAT_STB_INCLUDE_DIR=/path/to/stb \
  -DBONGO_CAT_NUKLEAR_INCLUDE_DIR=/path/to/nuklear \
  -DBONGO_CAT_MINIAUDIO_INCLUDE_DIR=/path/to/miniaudio
```

## 📌 项目状态

![Commit activity](https://img.shields.io/github/commit-activity/m/TianMengLucky/BongoCat-X?style=flat)
![Contributors](https://img.shields.io/github/contributors/TianMengLucky/BongoCat-X?style=flat)
![Stars](https://img.shields.io/github/stars/TianMengLucky/BongoCat-X?style=flat)
![Issues](https://img.shields.io/github/issues/TianMengLucky/BongoCat-X?style=flat)
![Release](https://img.shields.io/github/v/release/TianMengLucky/BongoCat-X?style=flat)

See [Insights / Pulse](https://github.com/TianMengLucky/BongoCat-X/pulse) for live charts.

![Alt](https://repobeats.axiom.co/api/embed/74334755a589dea40c5d31f8d2bcdc6c2bd39d87.svg "Repobeats analytics image")

## 📜 许可证

BongoCat 源代码和本地运行时采用 [AGPL-3.0-only](LICENSE) 许可证。

默认内置模型模式（`standard`）仍采用 MIT 许可证。`resources/assets/models/standard`、`keyboard` 和 `gamepad` 中捆绑的模型资源受单独的 [MIT 许可证声明](LICENSE-MIT) 覆盖。该 MIT 许可证仅适用于模型资源及其配套美术作品，不会改变 BongoCat 源代码或本地运行时的许可证。

## 🧭 技术架构

C11 宿主通过 SDL3 管理主线程事件、模型更新和呈现，设置界面使用 Nuklear。`src/rust/bongo-safe` 负责不可信数据解析、输入队列与指标以及通用插件工作线程状态。Live2D 桥接、Vulkan 资源后端和 Windows COM／WIC 适配器使用 C++17。

平台监听器将按键和鼠标按钮事件提交到 Rust 有界队列，单独合并指针移动，并唤醒主线程处理模型参数与 UI。模型渲染通过版本化 C ABI 加载 Live2D／Inox2D 插件，支持的平台后端为 OpenGL、Vulkan 和 Metal。

插件兼容性与构建细节见[渲染插件](docs/model-plugins.md)，原生后端限制及验证范围见[渲染说明](docs/live2d-vulkan-metal.md)。

## ❓ 常见问题

### 🔒 BongoCat 会记录我的键盘或鼠标输入吗？

BongoCat 在本地处理键鼠输入，以驱动动画和快捷键，不上传输入或使用数据。启用统计插件后，可在本地保存按键／鼠标汇总、每日／小时趋势及前台应用时长；不保存输入文本、按键顺序、窗口标题、网址或坐标。采集可在统计页关闭，历史数据可重置或导出。详见[统计插件说明](docs/statistics-plugin.md)。更新检查仅请求公开版本元数据。

### Linux Wayland 输入

X11 默认使用 XInput2。Wayland 的实验性 evdev 输入默认关闭；阅读
[输入权限风险](SECURITY.md#linux-input) 后，可以为单次启动显式启用：

```sh
BONGOCAT_ENABLE_EVDEV=1 ./build/BongoCat
```

启动时先以当前用户只读打开设备；权限不足时，受保护的安装会通过 `/usr/bin/sudo`
请求认证，打开设备后立即降回原用户并重新执行程序，再初始化 SDL、设置和模型。
自动提权要求程序、sudo 及其父目录归 root 所有且组与其他用户不可写；开发目录和
便携安装应由管理员单独授予设备访问权限。可用 `BONGO_CAT_SUDO_EXECUTABLE` 配置
可信 sudo 的绝对路径。认证或降权失败会停止启动；不会修改设备权限或安装 udev 规则。
仅监听启动时已有的设备，热插拔需要重启。不要直接以 root 运行或将账户加入 `input` 组。
原始输入可能包含密码框里的按键，监听不会在锁屏或切换会话时自动暂停。
退出程序才会停止监听，隐藏宠物不会停止；不带该变量重新启动即可恢复默认后端。
鼠标跟随使用未经加速的设备位移，窗口定位、点击穿透和置顶仍取决于 Wayland 合成器。

## 功能与文档

- **模型与渲染**：支持 Live2D、Inochi2D 和 Mver，多模型模式合并展示，支持多个 ZIP 后台排队导入。Windows 便携版请保留程序旁的 `plugins` 目录；渲染插件 ABI 为 v2，程序与插件需同步更新。见[渲染插件](docs/model-plugins.md)。
- **设置与外观**：支持紧凑布局、统一调色板、自定义图片背景和鼠标跟随范围；快捷键位于「偏好」子页，商店位于「模型」子页。设置自动保存。
- **统计**：键鼠总量、每日／每小时趋势、前台应用时长、自定义显示单位及 CSV 导出。数据保存在本地，可关闭采集或重置。见[统计插件](docs/statistics-plugin.md)。
- **物品**：图片批量导入、复制、位置／缩放／旋转／透明度、前后层级与场景保存；最多 16 个物品，仅作用于主桌宠，GIF 仅取首帧。见[物品插件](docs/items-plugin.md)。
- **音效**：按键绑定与试听 WAV／MP3／Ogg Vorbis／FLAC，动作音量可单独调整并保存。见[音效插件](docs/sounds-plugin.md)。
- **社区商店**：模型预览、作者链接、下载／卸载及 SHA-256 校验；付费条目可提供「购买完整版」链接。见[商店插件](docs/store-plugin.md)。
- **Mver 多按键气泡**：支持手部、外框与字符独立裁剪，镜像时字符保持可读。见[配置说明](docs/hand-layout.md)和[示例](docs/hand-layout.example.json)。
- **自动更新**：启动时异步检查，可在设置中关闭；发现新版本后可在设置窗口更新。开发版本见 [nightly 发布页](https://github.com/TianMengLucky/BongoCat-X/releases/tag/nightly)。

### 开发参考

[语言与职责边界](docs/language-migration.md) · [Vulkan 内存与生命周期](docs/vulkan-memory.md) · [Windows 覆盖呈现](docs/windows-overlay-presentation.md)

## 🙏 特别感谢
> [!TIP]
> BongoCat 的每一步都得益于开源精神。我们衷心感谢所有社区贡献者的无私奉献（按贡献日期先后排序列于下方）。正是你们的支持，让桌面陪伴更加自由与真诚。❤️‍🔥

<a href="https://github.com/TianMengLucky/BongoCat-X/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=TianMengLucky/BongoCat-X" />
</a>

---

<div align="center">
  版权所有 © 2026 - <strong>BongoCat</strong><br>
  By BongoCat-X Team<br>
  Fork maintained by <a href="https://github.com/TianMengLucky">TianMengLucky</a><br>
  Made with ❤️ &amp; ⌨️
</div>
