

2.2.3: optional right-click radial menu, larger settings window with maximize/restore controls, per-color reset buttons, and improved statistics unit editing and rounding.

<div align="center">
  <a href="https://bongocat.pet" target="_blank">
    <img src="../resources/assets/bongocat.png" alt="BongoCat" width="249">
  </a>
  
  <h1>
    <a href="https://bongocat.pet" target="_blank" style="text-decoration: none; color: inherit;">BongoCat</a>
  </h1>
</div>

<!-- Project Description + Rocket Icon -->
<p align="center"> 
 💘C/C++ × Rust × SDL3 — Three Mysterious Forces, United as One! Bong~ Bongocat!!!
</p>
<p align="center">
  <strong>English</strong> • <a href="../README.md">简体中文</a> • <a href="README.zh-Hant.md">繁體中文</a> • <a href="README.fr-FR.md">Français</a> • <a href="README.de-DE.md">Deutsch</a> • <a href="README.ko-KR.md">한국어</a> • <a href="README.pt-BR.md">Português</a> • <a href="README.ru-RU.md">Русский</a> • <a href="README.es-ES.md">Español</a> • <a href="README.id-ID.md">Bahasa Indonesia</a>
</p>
> [!NOTE]
> This repository (**BongoCat-X**) is a fork of [vladelaina/BongoCat](https://github.com/vladelaina/BongoCat). It is not affiliated with Live2D Inc. and does not bundle the Cubism SDK - see the Live2D Disclaimer section below.

<p align="center">
  <a href="../LICENSE"><img src="https://img.shields.io/badge/AGPL--3.0-1fa669?style=flat&logo=gnu&logoColor=white"></a>
  <a href="https://github.com/TianMengLucky/BongoCat-X"><img src="https://img.shields.io/badge/C-54AEFF?style=flat&logo=c&logoColor=white"></a>

</p>

<!-- Demo Video -->
<div align="center" style="margin-bottom: 30px;">
  <video src="https://github.com/user-attachments/assets/75719230-9e49-4124-ae5a-8e35592c5d49
" autoplay loop style="border-radius: 8px; max-width: 800px;"></video>
</div>

<img width="1149" height="904" alt="image" src="https://github.com/user-attachments/assets/aa376965-539e-4bbd-827d-bb9a29006069" />

> [!TIP]
> The model featured in this demonstration is from [宇痕冫](https://space.bilibili.com/348616056).
>
> 🎁 Looking for **free** models? We work with talented model creators to bring you a wide variety of free models, while continuously exploring more fun desktop experiences! Visit our official website: [bongocat.pet](https://bongocat.pet/models)
<p align="center">
  <a href="https://bongocat.pet/models">
    <img height="1080" src="https://github.com/user-attachments/assets/dedd83ca-742a-4f8c-a64a-69e659fca564" />
  </a>
</p>

<p align="center">
    <img src="https://count.getloli.com/@bongocat?name=bongocat&theme=booru-qualityhentais&padding=7&offset=0&align=top&scale=1&pixelated=1&darkmode=auto" width="400">
  </p>

## 💬 QQ community

Scan the QR code with QQ or search for group **211957388** to join the **BongoCat-X** community.

<p align="center"><img src="../resources/assets/qq-group.jpg" alt="BongoCat-X QQ · 211957388" width="320"></p>

## Live2D SDK and color settings

SDK import and download are managed in Settings → Plugins; the Models page manages models. On Windows, enable the Live2D plugin, choose Download Live2D SDK, then read and accept both official agreements to download and install the matching Core. Progress shows received bytes, total size and percentage; failures can be retried in the dialog. On other platforms, import a compatible Core manually. Appearance and solid-background colors share a palette popup opened by clicking the color swatch.

## 📥 Download

- GitHub Releases ([v2.2.3](https://github.com/TianMengLucky/BongoCat-X/releases/tag/v2.2.3))

  Download the latest release from [GitHub Releases](https://github.com/TianMengLucky/BongoCat-X/releases/latest).

  > [!IMPORTANT]
  > Official releases are **runtime-Core builds**: Live2D rendering support is built in, but the Live2D Cubism Core runtime is **not bundled** (proprietary license, never shipped with the packages). These are **not** diagnostic builds without Live2D. When no Core is found the app falls back to the diagnostic backend and shows a hint in the settings window.

  **Enable Live2D rendering (either way works):**

  1. **In-app import (recommended)**: open *Settings → Plugins* and click *Import Live2D Core*, then pick a `Live2DCubismCore.dll` or the official Cubism SDK zip. Takes effect immediately, no restart needed.
  2. **Drop into the live2d folder**: download **Cubism SDK for Native** from the [official download page](https://www.live2d.com/en/sdk/download/native/) (accept Live2D's license), then put the zip or the extracted `Live2DCubismCore.dll` into the `live2d` folder next to the app or inside the data directory and restart the app.

  For full SDK import steps when building from source, see the "Live2D / Cubism SDK" section below.

## ⚠️ Live2D Disclaimer

This repository is not affiliated with Live2D Inc. or its official projects in any way. The Live2D Cubism SDK is proprietary software of Live2D Inc.: it is not bundled, embedded, or distributed with this repository. To build with Live2D support, download and import it yourself from the official Live2D website as described in the "Live2D / Cubism SDK" section below, and follow Live2D's license terms.

## 🛠️ Build From Source

BongoCat requires CMake 3.24 or newer, C11 and C++17 compilers, desktop OpenGL development files, and the Rust toolchain (cargo, available through rustup). Corrosion builds the Rust modules. Configuration downloads SDL3, stb, miniaudio, Nuklear and Corrosion by default, so the first build needs network access.

Run the commands below from the project root (the directory containing
`CMakeLists.txt`).

### 📋 Platform Prerequisites

- **Windows:** Visual Studio 2022 with the Desktop C++ workload and CMake.
  Use the MSVC generator; MinGW can build the diagnostic backend but is not
  supported for the Cubism SDK.
- **macOS:** Xcode Command Line Tools, CMake, and Ninja. Select an architecture
  with `CMAKE_OSX_ARCHITECTURES` when it differs from the host default.
- **Linux (Debian/Ubuntu):** GCC or Clang, Ninja, and the OpenGL/X11 headers:

  ```bash
  sudo apt-get update
  sudo apt-get install -y build-essential cmake ninja-build \
    libgl1-mesa-dev libx11-dev libxi-dev libxfixes-dev libxcb-screensaver0-dev libfontconfig1-dev fonts-noto-cjk
  ```

### 🔧 Configure and Build

On Linux and macOS, use a single-configuration generator such as Ninja:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBONGO_CAT_FETCH_DEPS=ON
cmake --build build --parallel
```

On Windows, run from a Visual Studio 2022 developer shell (or another shell
where MSVC is available):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DBONGO_CAT_FETCH_DEPS=ON
cmake --build build --config Release --parallel
```

The executable is written to `build/BongoCat` on Linux, to
`build/BongoCat.app/Contents/MacOS/BongoCat` on macOS, and to
`build/Release/BongoCat.exe` for Visual Studio builds.

### 🧪 Tests

Test targets are disabled by default to reduce routine build work. Enable and build them when needed:

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

For a multi-configuration generator such as Visual Studio, select the build
configuration explicitly:

```powershell
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

### 🎭 Live2D / Cubism SDK (Optional — builds without it too)

The proprietary Cubism SDK remains optional. Without it, the host and Inox2D plugin still build. A compatible Live2D plugin plus user-supplied Cubism Core enables Live2D at runtime without rebuilding the host. SDK builds produce the separate C++ plugin.

1. Open the [Cubism SDK download page](https://www.live2d.com/en/sdk/download/native/),
   accept the Live2D Proprietary Software License Agreement, and download
   **Cubism SDK for Native** (releases are built against the `5-r.5` SDK).
2. Extract the archive. If the extracted folder is named
   `CubismSdkForNative-5-r.5`, rename it to `CubismSdkForNative` and place it
   under `vendor/` so that the tree contains `Core/` and `Framework/`.
3. Newer SDK archives no longer bundle GLEW. Download
   [GLEW 2.2.0](https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.zip)
   and extract it to `vendor/CubismSdkForNative/Samples/OpenGL/thirdParty/glew`
   (the directory that directly contains `include/GL/glew.h` and `src/glew.c`).

Alternatively, keep the SDK anywhere and pass its location explicitly:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBONGO_CAT_CUBISM_SDK=/path/to/CubismSdkForNative
```

The SDK must contain its Core library, Framework sources, and the OpenGL GLEW
third-party tree in the layout expected by `cmake/Cubism.cmake`. Windows
Cubism builds require Visual Studio 2022. Once the SDK is in place, configure
as usual to get a build with Live2D rendering; `BONGO_CAT_REQUIRE_CUBISM=ON`
only makes configuration fail fast with import instructions when the SDK is
missing (release CI uses it). Leave it at the default `OFF` when you do not
need that.

### ⚙️ CMake Options

| Option | Default | Description |
| --- | --- | --- |
| `BONGO_CAT_FETCH_DEPS` | `ON` | Download the pinned third-party dependencies with CMake `FetchContent` (including Corrosion and the Rust crate dependencies). Set `OFF` only when SDL3, stb, miniaudio, Nuklear, and Corrosion are already available to CMake. |
| `BONGO_CAT_CUBISM_SDK` | `vendor/CubismSdkForNative` | Path to the Cubism SDK for Native. |
| `BONGO_CAT_REQUIRE_CUBISM` | `OFF` | Whether a missing SDK fails configuration. Default `OFF`: a missing SDK builds the diagnostic backend without Live2D rendering; set `ON` to require the SDK (release CI uses it). |
| `BONGO_CAT_WARNINGS_AS_ERRORS` | `OFF` | Treat native compiler warnings as errors. |

For an offline build with `BONGO_CAT_FETCH_DEPS=OFF`, provide CMake package
configurations for SDL3 (including `SDL3-static`) , plus the include
directories for stb, Nuklear, and miniaudio when they are not discoverable:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBONGO_CAT_FETCH_DEPS=OFF \
  -DBONGO_CAT_STB_INCLUDE_DIR=/path/to/stb \
  -DBONGO_CAT_NUKLEAR_INCLUDE_DIR=/path/to/nuklear \
  -DBONGO_CAT_MINIAUDIO_INCLUDE_DIR=/path/to/miniaudio
```

## 📌 Project Status

![Commit activity](https://img.shields.io/github/commit-activity/m/TianMengLucky/BongoCat-X?style=flat)
![Contributors](https://img.shields.io/github/contributors/TianMengLucky/BongoCat-X?style=flat)
![Stars](https://img.shields.io/github/stars/TianMengLucky/BongoCat-X?style=flat)
![Issues](https://img.shields.io/github/issues/TianMengLucky/BongoCat-X?style=flat)
![Release](https://img.shields.io/github/v/release/TianMengLucky/BongoCat-X?style=flat)

See [Insights / Pulse](https://github.com/TianMengLucky/BongoCat-X/pulse) for live charts.

![Alt](https://repobeats.axiom.co/api/embed/74334755a589dea40c5d31f8d2bcdc6c2bd39d87.svg "Repobeats analytics image")

## 📜 License

The BongoCat source code and native runtime are licensed under
[AGPL-3.0-only](../LICENSE).

The default built-in model mode (`standard`) remains MIT-licensed. The
bundled model assets in `resources/assets/models/standard`, `keyboard`, and
`gamepad` are covered by the separate [MIT license notice](../LICENSE-MIT).
That MIT license applies to the model assets and their accompanying artwork
only; it does not relicense the BongoCat source code or native runtime.

## 🧭 Technical Architecture

The C11 host uses SDL3 for main-thread events, model updates and presentation, with Nuklear for settings. `src/rust/bongo-safe` owns untrusted parsing, input queues and metrics, and shared plugin worker state. The Live2D bridge, Vulkan resource backend and Windows COM/WIC adapters use C++17.

Platform listeners submit key and mouse-button events to a bounded Rust queue, coalesce pointer motion separately, and wake the main thread to update model parameters and UI. A versioned C ABI loads Live2D/Inox2D renderer plugins, using OpenGL, Vulkan or Metal where supported.

See [renderer plugins](model-plugins.md) for compatibility and build details, and [native rendering](live2d-vulkan-metal.md) for backend limits and validation scope.

## ❓ FAQ

### 🔒 Does BongoCat record my keyboard or mouse input?

BongoCat processes keyboard and mouse input locally for animations and shortcuts and does not upload input or usage data. When enabled, the statistics plugin can save local aggregate counts, daily/hourly trends and foreground application time. It does not save typed text, key order, window titles, URLs or coordinates. Collection can be disabled and history reset or exported on the Statistics page. See [Statistics plugin](statistics-plugin.md). Update checks request public release metadata.

### Linux Wayland Input

X11 uses XInput2 by default. Experimental evdev input for Wayland is off by
default. After reviewing [the input permission risks](../SECURITY.md#linux-input),
it can be explicitly selected for one launch:

```sh
BONGOCAT_ENABLE_EVDEV=1 ./build/BongoCat
```

Startup first tries read-only device access as the current user. On permission
denial, protected installations request authentication through `/usr/bin/sudo`,
open devices, then drop privileges and re-execute before SDL, settings, models
or plugins initialize. Automatic elevation requires root-owned binaries and
parent directories not writable by group or other users. Development/portable
installs need separately administered device access. Configure a trusted sudo
absolute path with `BONGO_CAT_SUDO_EXECUTABLE`. Authentication or privilege-drop
failure stops startup. No device permissions or udev rules are changed.
Only startup devices are monitored; hot-plug requires a restart. Do not launch
the app directly as root or add your account to the `input` group. Raw input can include
password keystrokes and is not paused on screen lock or session switching.
Close the app to stop monitoring; hiding it does not stop input. Launch
without the variable to return to the default backend. Evdev mouse following
uses unaccelerated device motion; Wayland placement, click-through, and
always-on-top support still depend on the compositor.

### 🖼️ What is the Vulkan / Metal support status?

Settings → Application → Render Backend switches immediately: OpenGL ↔ Vulkan on Windows / Linux x64, OpenGL ↔ Metal on macOS. System defaults to OpenGL. Native texture uploads, Cubism drawing, GPU readback and model reload are connected; switching preserves the model and selected motions/expressions, with OpenGL recovery on initialization or reload failure. Vulkan/Metal remain experimental: key/effect/pointer overlays now support all backends. Live2D supports dynamic texture resolution on OpenGL, Vulkan and Metal, with background decoding/resizing and synchronous native GPU uploads. Disabling it reloads textures at the quality-selected resolution. Native backends cache static desk backgrounds and include them in the tight-frame bounds; GPU corner masks draw only four corners without an extra full-frame texture. Overlays reuse SDL software rendering, cached until input changes, then composite into the completed native frame and upload to the GPU; performance remains unmeasured.

Large Rendering Optimization appears with Vulkan / Metal and is off by default. Vulkan offers three independent options; Metal offers only large allocations. Changes reload the model. Inox2D Metal packs frame vertices into a shared buffer when enabled and allocates per node when disabled; Live2D Metal reports this option as unavailable. Inox2D Vulkan supports parallel command recording and large memory blocks. Live2D Vulkan supports VMA allocations and parallel recording for ordinary blending without offscreen nodes or high-precision masks. Live2D needs at least 128 Drawables for secondary buffers and 128 drawable snapshots for up to four workers; other models retain the SDK serial path. Live2D Vulkan Bindless uses a table of up to 64 atlas textures for ordinary blending without offscreen nodes, with fallback when device features or capacity are insufficient. Inox2D Bindless is not implemented. Larger pools may use more memory; performance gains remain unmeasured.

SDK builds default to `BONGO_CAT_CUBISM_VULKAN=ON` on Windows / Linux x64 (requires `glslangValidator` or `glslang`; install `glslang-tools` on Linux; Vulkan 1.3 at runtime) and `BONGO_CAT_CUBISM_METAL=ON` on macOS (Xcode Metal tools). Set the relevant option to `OFF` for OpenGL builds. Metal sources and `.metallib` files are included only on macOS; Vulkan sources and `.spv` files only on Windows / Linux x64, including blend variants. Diagnostic builds without the SDK omit native Live2D shader resources. GitHub Actions prepares platform tools and checks resource selection. Windows builds and GPU regression tests pass. Metal Rust APIs pass cross-target type checking; macOS GPU validation is pending. See [integration status and validation](live2d-vulkan-metal.md).

## 🙏 Special Thanks
> [!TIP]
> Every step BongoCat takes is powered by the spirit of open source. We sincerely thank all our community contributors for their selfless contributions It is your support that makes desktop companionship more free and genuine.❤️‍🔥

<a href="https://github.com/TianMengLucky/BongoCat-X/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=TianMengLucky/BongoCat-X" />
</a>

---

<div align="center">

Copyright © 2026 - **BongoCat**\
By vladelaina\
Fork maintained by [TianMengLucky](https://github.com/TianMengLucky)\
Made with ❤️ & ⌨️

</div>

### Preferences options layout

Preferences uses collapsible group cards, with Window, Model, Application and Appearance expanded by default. Enable Compact layout under Appearance to reduce spacing while keeping descriptions visible. The setting is saved automatically.

### Input statistics plugin

The Rust statistics plugin builds with the application by default. Enable, disable or import it on the Plugins page. The Statistics page shows keyboard and mouse totals, daily and hourly charts, local persistence and CSV export. See [Statistics plugin](statistics-plugin.md). Foreground application time includes rankings, hourly totals and a separate collection switch. Sampling, storage and export run on an independent worker. The plugin registers the Statistics entry when enabled and removes it when disabled or uninstalled.

Customize unit labels and travel/scroll multipliers on the Statistics page, with saved settings and restore defaults. Appearance uses one shared palette for primary, secondary, text and page background colors.

### Dynamic renderer plugins

Preferences → Plugins manages local renderer installation, activation and removal. Switch to another renderer before changing an active plugin. Live2D uses a C++ plugin; Inochi2D uses the Rust Inox2D plugin with OpenGL, Vulkan (Windows/Linux) and Metal (macOS), without wgpu. Import identifies model contents first (`.model3.json`, `.inp`, `.inx`). All JSON parsing and serialization is performed in Rust.

Windows portable releases are ZIP archives. Keep the extracted `plugins` directory beside the executable. See [Dynamic renderer plugins](model-plugins.md) for packaging, offline dependency paths, plugin ABI, parameter bindings and Inox2D compatibility limits.

Each `dev` push that changes code, assets or build configuration publishes a GitHub nightly prerelease. Documentation-only pushes are skipped; there is no scheduled trigger. All platforms build the pushed commit. Nightlies do not replace the latest stable release.

Nightly prereleases reuse the [nightly release page](https://github.com/TianMengLucky/BongoCat-X/releases/tag/nightly), update its tag and replace assets with stable names instead of creating a release for each build. Failed platforms retain their previous assets.

Preferences → Preferences → Window includes Custom Image Background. Choose Image imports PNG, JPEG, WebP, BMP or GIF (first frame). The image fills the window centered while preserving its aspect ratio; enabling it disables Solid Background and vice versa. Imported images are stored in the application data directory and remain available after the original is moved. Limits: 64 MB and 16 megapixels per image. OpenGL, Vulkan and Metal are supported.

Live2D automatically reuses geometry buffers and repeated render state. OpenGL streaming, Vulkan upload batching and pipeline caching, Metal shared buffer slots, and native adaptive masks require no additional switch. Performance and platform behavior of these new paths remain to be measured.

Vulkan manages memory automatically: Live2D uses 1 MiB VMA blocks by default, or 16 MiB with explicit large allocation. Windows layered presentation keeps one color target. Hidden or minimized windows release readback caches after GPU completion and recreate them on demand. Texture quality and dynamic resolution remain independent settings.

Settings → Pet → Custom Mouse Follow Range provides horizontal/vertical 25–400%, default off; 100% keeps the original range. Drawn Mver hands and mice can move farther; Live2D retains authored parameter limits. Gaze and window position are unchanged

### Multiple keys and readable mirrored characters

Mver (OpenGL/Vulkan/Metal) crops the hand, bubble frame and characters separately, joining held keys with + inside one bubble. The frame and tail mirror with the model while characters stay readable, also in single-key mode. Configure the three crop regions, spacing around + and per-side display limit in `hand-layout.json` beside the hand images; restart after editing. See the [configuration guide and example](hand-layout.md).

The action dialog checks each motion’s `Sound` reference and whether the audio file exists, then shows an individual volume control beside motions with audio. Set 0–100% by dragging or typing; double-click restores 100%, and 0% mutes. Settings persist per model and motion and apply to dialog playback, shortcuts, random motions and the context menu, including live adjustment. Other motions sharing the same clip keep their own volume.

### Items plugin

The independent Items plugin is bundled by default. Enable it on Preferences → Plugins to show the Items page. It supports batch image import/drop, multiple props, position, scale, rotation, opacity, front/back layers, locks, duplicates and scene saving on OpenGL, Vulkan and Metal. Viewport following is not mesh pinning; GIF uses its first frame, with up to 16 items on the primary pet. See [Items plugin](items-plugin.md). Set `BONGO_CAT_BUILD_ITEMS=OFF` to omit the bundled library.

Render memory preference: Video memory first / System memory first. Live2D Vulkan only. Reloads rendering. Prefer video memory for speed or system memory to reduce VRAM pressure. Unsupported resources fall back automatically; system memory may be slower. Other backends allocate automatically; integrated GPUs share memory.

### Keyboard sound effects plugin

Enable Sound effects on Plugins. Click a keyboard key to choose audio, hover to see its filename, and press the physical key to play it. Bindings save automatically. Sounds and Items each have a separate persistent worker. WAV/MP3/Ogg Vorbis/FLAC are supported on the primary pet. See [Sound effects plugin](sounds-plugin.md); `BONGO_CAT_BUILD_SOUNDS=OFF` omits the library.

The Rust model store plugin shows preview cards, author links, and download/uninstall buttons. Downloads must pass SHA-256 verification before installation. The store shows a reserved state until the catalog is published. See [store plugin](store-plugin.md).

Automatically Check for Updates is enabled by default under Preferences → Application. Each launch checks once in the background. New-version notifications stay available until Settings opens and offer an Update now button in the top-right corner. Every settings toast can be dismissed manually.

The Models page groups modes from the same package into one card with a mode selector, including the default Standard, Keyboard and Gamepad modes. Drop several model ZIPs to queue extraction, discovery and installation in the background. Closing Settings keeps imports running. Quitting stops subsequent imports and waits for the current source to finish before releasing workers and temporary resources.

Shortcuts is now a subpage of Preferences: enter Preferences, then switch Settings / Shortcuts at the top. Store is a subpage of Model: enter Model, then switch Local models / Store. These two standalone sidebar entries are removed; each subpage retains its own scroll position.

Enabling Live2D without Cubism Core opens both official license agreements. On Windows, Agree and download fetches the Native R5 SDK over official HTTPS, extracts only the matching dynamic Core and enables rendering locally. Cancel and continue keeps the plugin enabled without rendering. Failed downloads can be retried, and manual import remains available. The application and packages contain no SDK or Core.

Windows AMD graphics offer experimental DirectComposition presentation, off by default. DXGI alpha composition applies immediately, throttles expose events to 50 ms and disables VSync; failures restore the previous presenter. Pixel readback remains, and game Independent Flip is not guaranteed.

Renderer plugin ABI is now v2: update or rebuild the application and Live2D/Inox2D plugins together; older plugins are incompatible. Live2D Vulkan supports on-demand offscreen resources, Rust streaming PNG scaling and BC7 caching with RGBA8 fallback; see [native rendering details](live2d-vulkan-metal.md) for platform limits.
