

2.2.0 adds items, keyboard sounds and model store plugins, with improved background imports and automatic updates. Screen edge bounce requires Keep on screen. Windows OpenGL true transparency uses per-pixel alpha and may increase pixel readback overhead.

<div align="center">
  <a href="https://bongocat.pet" target="_blank">
    <img src="resources/assets/bongocat.png" alt="BongoCat" width="249">
  </a>
  
  <h1>
    <a href="https://bongocat.pet" target="_blank" style="text-decoration: none; color: inherit;">BongoCat</a>
  </h1>
</div>

<!-- Project Description + Rocket Icon -->
<p align="center"> 
 💘C × SDL3 × OpenGL — Three Mysterious Forces, United as One! Bong~ Bongocat!!!
</p>
<p align="center">
<strong>English</strong> • <a href="../README.md">简体中文</a> • <a href="README.zh-Hant.md">繁體中文</a> • <a href="README.fr-FR.md">Français</a> • <a href="README.de-DE.md">Deutsch</a> • <a href="README.ko-KR.md">한국어</a> • <a href="README.pt-BR.md">Português</a> • <a href="README.ru-RU.md">Русский</a> • <a href="README.es-ES.md">Español</a> • <a href="README.id-ID.md">Bahasa Indonesia</a>
</p>
> [!NOTE]
> This repository (**BongoCat-X**) is a fork of [vladelaina/BongoCat](https://github.com/vladelaina/BongoCat). It is not affiliated with Live2D Inc. and does not bundle the Cubism SDK - see the Live2D Disclaimer section below.

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/AGPL--3.0-1fa669?style=flat&logo=gnu&logoColor=white"></a>
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

## 📥 Download

- GitHub Releases ([v2.2.0](https://github.com/TianMengLucky/BongoCat-X/releases/tag/v2.2.0))

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

BongoCat uses CMake and requires a C11 compiler, a C++17 compiler, CMake 3.24
or newer, desktop OpenGL development files, and a Rust toolchain (cargo, e.g.
via rustup): the memory-safety-critical parsers (SHA-256, image decoding, the
contributor feed, audio decoding) live in the `src/rust/bongo-safe` crate,
which Corrosion builds at configure time. SDL3, stb, miniaudio,
and Nuklear are downloaded at configure time by default, so the first
configuration needs network access.

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

CTest targets are enabled by default. Run them after building:

```bash
ctest --test-dir build --output-on-failure
```

For a multi-configuration generator such as Visual Studio, select the build
configuration explicitly:

```powershell
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

> [!TIP]
> On Windows you can enable Live2D rendering without rebuilding: open
> Settings → Model in the app, click "Import Live2D Core" and select a
> `Live2DCubismCore.dll` or the official Cubism SDK zip. The change takes
> effect immediately, no restart needed.

> [!NOTE]
> Official releases of this repository are runtime-Core builds: they include
> the Live2D renderer but **do not bundle the Core runtime**. On startup the
> app checks the `live2d` folder — drop `Live2DCubismCore.dll` or the official
> SDK zip into it (next to the application or inside the data directory) and
> it is picked up automatically after a restart; you can also click "Import
> Live2D Core" under Settings → Plugins to activate it immediately. When no
> Core is found the app falls back to the diagnostic backend and shows a hint
> in the settings window.

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
[AGPL-3.0-only](LICENSE).

The default built-in model mode (`standard`) remains MIT-licensed. The
bundled model assets in `resources/assets/models/standard`, `keyboard`, and
`gamepad` are covered by the separate [MIT license notice](LICENSE-MIT).
That MIT license applies to the model assets and their accompanying artwork
only; it does not relicense the BongoCat source code or native runtime.

## 🧭 Technical Architecture

> The current native version is built on C/C++, SDL3, and OpenGL. The diagram below focuses on the runtime data flow; build and packaging details live in CMake.

### 🔄 Runtime Ownership and Frame Scheduling

Each process owns one `BongoCatApp` and one main-thread event and render loop.
Platform listeners stop at the input boundary:

```text
Platform listeners
(keyboard / pointer)
            |
            v
  C11 input state
  (atomic edge queue + coalesced pointer position)
            |
            v
  main-thread application <----- SDL3 events
            |
            v
  model parameters, overlay, and UI state
            |
            v
  model update -> OpenGL composition -> platform presentation
```

The Windows Raw Input receiver, macOS Quartz event tap, and Linux XInput2 listener
run outside the main loop. They publish timestamped key and mouse-button edges
to the bounded atomic queue and coalesce pointer motion separately, so frequent
motion cannot displace ordered key and button edges. Native SDL wake events
notify the main thread. Windows registers a message-only receiver for background
keyboard and mouse input with `RIDEV_INPUTSINK | RIDEV_DEVNOTIFY`, preserving legacy
window messages. Device motion drives the model when another application hides
or locks the cursor; SDL supplies the desktop cursor position. The receiver
tracks held inputs per device and clears them on device removal or desktop
switches. It does not install input hooks, use DirectInput, or send input to games.
SDL3 window, preferences, and gamepad events
are handled on the main thread, where gamepad events are normalized before they
reach model parameters or shortcuts. No platform listener calls Live2D,
overlay, or UI code directly.

`bongo_cat_app_run` handles update-shutdown and secondary-process arguments,
enforces single-instance ownership for the primary process, allocates the
application state, runs initialization, enters `bongo_cat_app_loop`, and then
flushes state and destroys resources in a defined order. Initialization loads
configuration and storage paths, locates assets, creates the SDL/OpenGL pet
window, initializes the platform backend, creates the Live2D, overlay, and
audio services, scans the built-in/installed/nearby model sources, and loads a
usable model. `BongoCatApp` owns settings, session state, model and behavior
catalogs, platform handles, and runtime service handles.

Installed model packages use Mver as the canonical format. The import workflow
resolves a selected file or directory, discovers and validates candidates,
fingerprints package identity, converts Tauri sources to Mver, applies image
patches, and commits the normalized package under `models_root`. It then
generates the runtime adapter and refreshes the catalogs. Nearby sources are
discovered without installing their source tree; their adapters and inspection
results are cached outside `models_root` under `cache_root`.

Each main-loop iteration waits for SDL/native wakeups or the earliest pending
frame, UI, animation, or pointer-hit deadline (with a maximum wait of 250 ms).
It dispatches queued SDL events, drains the atomic input queue and release
recovery, updates window and model-refresh state, and applies input-derived
parameters. With Cubism enabled, the model deadline follows
`settings.model.max_fps` (60 FPS by default); diagnostic builds use a 100 ms
fallback interval. The elapsed model time is capped at 250 ms and split into up
to eight substeps, targeting no more than 1/30 s per substep.

The normal pet path renders only when the window is visible, not minimized, and
marked dirty. A frame clears the background, draws the model, and composites
pointer, key, and effect overlays before calling the platform presenter.
Preview operations can request immediate renders, while capture renders may
skip presentation. macOS and Linux swap the SDL OpenGL window directly.
Windows swaps directly when layered presentation is inactive and otherwise
reads back the frame for `UpdateLayeredWindow`. The preferences UI owns a
separate SDL/OpenGL window and is rendered and presented independently from
the pet window.

The C runtime calls the ABI declared in `include/bongo_cat/model.h`. The Live2D
bridge and Cubism implementation live in `src/live2d` and use C++17 only when
the Cubism SDK is enabled; the rest of the native runtime uses C11. Cubism
types remain behind opaque C handles, while `src/live2d/live2d_stub.c` provides
the diagnostic backend when the SDK is unavailable.

```mermaid
flowchart TB
  Input(["Keyboard / mouse / gamepad"])
  BuiltIn(["Built-in model assets"])
  Sources(["External model sources<br/>Mver, Tauri, .model3.json, image patches"])
  Desktop(["Pet window and preferences window"])

  subgraph Runtime["BongoCat native runtime"]
    direction TB
    Entry["src/main.c<br/>bongo_cat_app_run"]
    Startup["Startup and initialization<br/>configuration, storage, window, platform"]
    Loop["SDL3 main loop<br/>wait, dispatch, update, render"]
    Shutdown["Shutdown<br/>flush state, stop services, release resources"]
    InputQueue[("Atomic input state<br/>edge queue and coalesced pointer position")]
    InputDispatch["Input dispatch<br/>shortcuts, pointer mapping, model parameters"]
    State[("BongoCatApp state<br/>settings, session, catalogs, runtime handles")]
    Import["Model discovery and import<br/>validate, normalize to Mver, install/cache"]
    Catalog[("Model and behavior catalogs")]
    Live2D["Live2D C ABI<br/>Cubism SDK or diagnostic stub"]
    Overlay["Overlay and audio"]
    Preferences["Preferences and desktop shell<br/>Nuklear UI, tray, window actions"]
    Compose["OpenGL frame composition"]
    Present["Platform presentation"]

    Entry --> Startup --> Loop
    Loop --> Shutdown
    Loop --> InputDispatch --> State
    Loop <--> State
    State --> Live2D
    State --> Overlay
    State <--> Preferences
    Loop --> Preferences
    Catalog --> State
    State --> Compose
    Live2D --> Compose
    Overlay --> Compose
    Compose --> Present
    Loop --> Compose
  end

  subgraph Platform["Platform backends"]
    direction LR
    Global["Global keyboard / pointer capture<br/>Windows, macOS, Linux"]
    SDL["SDL3 events<br/>window and gamepad events"]
  end

  Input --> Global --> InputQueue --> InputDispatch
  Input --> SDL --> Loop
  BuiltIn --> Catalog
  Sources --> Import --> Catalog
  Present --> Desktop
  Preferences --> Desktop
```

## ❓ FAQ

### 🔒 Does BongoCat record my keyboard or mouse input?

No. BongoCat processes keyboard and mouse input locally to drive animations
and shortcuts. It does not record or upload your keystrokes, mouse actions, or
other interaction data. Configuration is stored locally as well, and the app
contains no ads, analytics tools, or user-tracking code. When an update check
is performed, it only requests public release metadata; it does not send input,
configuration, or usage data.

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

Preferences uses one grouped card layout, with Window, Model, Application and Appearance expanded by default. Click a group title to collapse or expand its options. There is no layout switch; existing setting values are preserved.

### Input statistics plugin

The Rust statistics plugin builds with the application by default. Enable, disable or import it on the Plugins page. The Statistics page shows keyboard and mouse totals, daily and hourly charts, local persistence and CSV export. See [Statistics plugin](statistics-plugin.md). Foreground application time includes rankings, hourly totals and a separate collection switch. Sampling, storage and export run on an independent worker. The plugin registers the Statistics entry when enabled and removes it when disabled or uninstalled.

### Dynamic renderer plugins

Preferences → Plugins manages local renderer installation, activation and removal. Switch to another renderer before changing an active plugin. Live2D uses a C++ plugin; Inochi2D uses the Rust Inox2D plugin with OpenGL, Vulkan (Windows/Linux) and Metal (macOS), without wgpu. Import identifies model contents first (`.model3.json`, `.inp`, `.inx`). All JSON parsing and serialization is performed in Rust.

Windows portable releases are ZIP archives. Keep the extracted `plugins` directory beside the executable. See [Dynamic renderer plugins](model-plugins.md) for packaging, offline dependency paths, plugin ABI, parameter bindings and Inox2D compatibility limits.

Each `dev` push that changes code, assets or build configuration publishes a GitHub nightly prerelease. Documentation-only pushes are skipped; there is no scheduled trigger. All platforms build the pushed commit. Nightlies do not replace the latest stable release.

Nightly prereleases reuse the [nightly release page](https://github.com/TianMengLucky/BongoCat-X/releases/tag/nightly), update its tag and replace assets with stable names instead of creating a release for each build. Failed platforms retain their previous assets.

Preferences → Preferences → Window includes Custom Image Background. Choose Image imports PNG, JPEG, WebP, BMP or GIF (first frame). The image fills the window centered while preserving its aspect ratio; enabling it disables Solid Background and vice versa. Imported images are stored in the application data directory and remain available after the original is moved. Limits: 64 MB and 16 megapixels per image. OpenGL, Vulkan and Metal are supported.

Live2D automatically reuses geometry buffers and repeated render state. OpenGL streaming, Vulkan upload batching and pipeline caching, Metal shared buffer slots, and native adaptive masks require no additional switch. Performance and platform behavior of these new paths remain to be measured.

Settings → Application → Low Memory Mode is off by default. It caps texture quality at 10%, edges at 512 pixels and Live2D masks at 256 pixels, suspends large rendering optimizations and reloads the renderer. Disabling restores saved quality; memory depends on the model and driver. 50MB is a target, not a guarantee.

Settings → Pet → Custom Mouse Follow Range provides horizontal/vertical 25–400%, default off; 100% keeps the original range. Drawn Mver hands and mice can move farther; Live2D retains authored parameter limits. Gaze and window position are unchanged. Low Memory Mode disables OpenGL MSAA and reduces unused OpenGL shaders and Vulkan blend pipelines.

On Windows, low-memory settings use chunked SDL software drawing and an opaque background for OpenGL/Vulkan. Fonts retain their actual sizes, display DPI and antialiasing. The software atlas stores A8 coverage and converts only the glyph being drawn. OpenGL on other platforms continues to reuse the pet context.
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
