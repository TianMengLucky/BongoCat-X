# AGENTS.md

Guidance for AI coding agents (and new contributors) working in this
repository. Follow these rules; when a task conflicts with them, surface
the conflict instead of silently working around it.

## Project Overview

BongoCat is a cross-platform (Windows / macOS / Linux) Live2D / Inochi2D desktop pet
written in C11 with a C++17 Live2D bridge. It uses SDL3, OpenGL, Nuklear,
Rust JSON libraries, and optional dynamically loaded model renderers. Build system is CMake (>= 3.24).
A Rust toolchain (cargo) is a build requirement: the memory-safety-critical
parsers (SHA-256, image decoding, the contributor feed, audio decoding,
all JSON, INP/INX containers, Live2D expression files) live in the `bongo-safe` crate under `src/rust/`,
built via Corrosion.

Key directories:

- `src/core/` — C11 core: config I/O, model catalog, i18n, paths, input state.
- `src/live2d/` — Live2D bridge. C++17 (`cubism_*.cpp`) when built with the
  Cubism SDK; `live2d_stub.c` is the diagnostic fallback used without it.
  The host dispatches through the versioned ABI in `include/bongo_cat/model_plugin.h`.
- `src/rust/bongo-inox2d/` — Inox2D Rust plugin: upstream OpenGL, ash Vulkan,
  metal Metal; Naga translates shared WGSL to SPIR-V/MSL.
- `src/rust/bongo-safe/` — Rust crate behind `include/bongo_cat/safe_ffi.h`;
  every untrusted byte stream is parsed here, not in C — including the
  settings/session configuration JSON (`config.rs`).
- `src/runtime/` — app lifecycle, shell (tray, menus), model import, updates.
- `src/ui/` — Nuklear-based preferences window (`preferences_*`), overlays.
- `src/platform/` — per-platform backends (windows, macos, linux).
- `include/bongo_cat/` — public C headers; keep the ABI stable.
- `resources/` — app assets. `resources/icons/` (ico/icns), `resources/assets/`
  (embedded via the asset packer), `resources/assets/locales/` (i18n JSON).
- `cmake/`, `packaging/`, `tests/`, `docs/`, `.github/`.

## Live2D Cubism SDK Constraint (important)

The Cubism SDK is proprietary and is **not** committed here — it is
gitignored. The SDK is optional at build time: `BONGO_CAT_REQUIRE_CUBISM`
defaults to `OFF`. Without the SDK the host and Inox2D plugin still build;
Live2D falls back to diagnostic rendering until a compatible C++ plugin is
installed and user-supplied Core is available. With the SDK, CMake builds
`bongo_live2d` as a separate shared library. The host never links Framework.
The plugin never links or embeds Cubism Core; the host loads user-supplied Core
externally. Static Core linking is unsupported, and packages must contain no SDK.
Plugins are managed on the preferences Plugins page, in `<data>/plugins`;
bundled plugins live beside the executable or in macOS `Contents/PlugIns`.
Set `BONGO_CAT_REQUIRE_CUBISM=ON` to fail
configuration with import instructions when the SDK is missing (the
release CI does). Never add SDK sources/binaries to the repository or to
release artifacts of jobs that are not guarded for it
(`.github/scripts/check-publish-guards.ps1` enforces this in CI).

## Build & Test

Windows (MSVC, Visual Studio 2022 or newer):

```bat
build.bat                 :: Release, no Cubism SDK needed (diagnostic backend)
set BONGOCAT_REQUIRE_CUBISM=1 && build.bat   :: require the SDK (Live2D build)
ctest --test-dir build-cubism -C Release --output-on-failure
```

Unix (Ninja):

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Notes:

- Plain CMake configures fine without the wrapper; the SDK is optional by
  default, and a missing SDK builds the diagnostic backend with a warning
  instead of failing (pass `-DBONGO_CAT_REQUIRE_CUBISM=ON` to fail with
  import instructions).
- The local Windows test build directory convention is `build-tests/`
  (`build*/` is gitignored).
- Dependencies (SDL3, stb, miniaudio, Nuklear, Corrosion and the
  Rust crate dependencies) are fetched by CMake `FetchContent`; do not vendor
  them. `cargo test --manifest-path src/rust/bongo-safe/Cargo.toml` runs the
  crate's unit tests.

## CI Gates — run these before finishing any change

- **File size policy:** no source file over 500 lines
  (`cmake -DROOT=. -P cmake/CheckLines.cmake`).
- **cppcheck** must stay clean for `src/` (see the `quality` job in
  `.github/workflows/ci.yml` for the exact invocation).
- **Legacy product name:** the string `l2d` + `cat` (case-insensitive) must
  not appear anywhere in the repo.
- **Publishing guards:** if you touch workflows, run
  `pwsh .github/scripts/check-publish-guards.ps1 -SelfTest`.
- Build with `-DBONGO_CAT_WARNINGS_AS_ERRORS=ON` before pushing risky
  changes; `/W4` (MSVC) and `-Wall -Wextra` (GCC/Clang) are the baseline.

## Conventions

- **Reuse dependencies:** When an existing library or dependency meets the
  need, use it directly instead of reimplementing its functionality. Keep
  custom code to necessary integration and narrowly scoped upstream fixes.

- **C11, no extensions** for C sources; C++17 only under `src/live2d/`.
  Cubism types stay behind opaque C handles.
- **i18n:** user-facing strings go through `bongo_cat_i18n_get` with keys in
  `resources/assets/locales/*.json`. There are ten locales (en-US, zh-CN,
  zh-Hant, ja-JP, ko-KR, de-DE, es-ES, fr-FR, pt-BR, ru-RU) — when adding or
  changing a key, update **all** of them in the same commit.
- **Logging:** SDL3 (`SDL_Log*`). At startup `src/runtime/lifecycle/startup.c`
  installs a custom output function (`log_output`) that appends to
  `<data>/state/runtime-diagnostics.log` and mirrors to stderr with a
  `[time] [source] [PRIORITY:category]` prefix. Filtering lives in
  `include/bongo_cat/log.h` (`bongo_cat_log_enabled`): WARN and above always
  pass, but **INFO is dropped unless the category is one of the app's own**
  (`BONGO_CAT_LOG_LIFECYCLE` = `SDL_LOG_CATEGORY_CUSTOM`, `BONGO_CAT_LOG_UPDATE`,
  `BONGO_CAT_LOG_INPUT`). Messages logged with plain
  `SDL_LOG_CATEGORY_APPLICATION` are visible only in the brief window before
  the sink installs (very early startup) and are silently discarded afterwards
  — use the `BONGO_CAT_LOG_*` categories for anything meant to be seen at
  runtime.
- **SDL memory ownership (SDL2 â SDL3 migration hazard):** several SDL3
  functions return strings/objects owned by SDL that must **not** be freed —
  unlike SDL2. The known trap: `SDL_GetBasePath()` now returns an internal
  cached pointer; `SDL_free()`-ing it corrupts the heap and crashes the next
  caller of the same function (double free). Audit every `SDL_free` /
  `free` against the SDL3 API docs when touching platform or path code.
- **Docs:** the root `README.md` is Simplified Chinese; `docs/README.en-US.md`
  mirrors it in English alongside the other translations
  (`docs/README.<lang>.md`). `CHANGELOG.md` records the fork's changes vs
  upstream. Keep build/feature instructions consistent across languages when
  they change.
- **UI toasts** in the settings window go through
  `bongo_cat_preferences_notice_show[_anchored]` in
  `src/ui/preferences/preferences_notice.c` — do not invent ad-hoc popups.
- **Line endings:** sources use CRLF in the working tree; keep conversions
  byte-stable when scripting edits (prefer reading/writing bytes).
- **Temp scripts:** do not leave scratch files in the repo root; use a
  gitignored `build*/` directory and delete them when done.
- **Commits:** short imperative subjects with conventional prefixes
  (`feat:`, `fix:`, `chore:`, `docs:`), body only when the why is not
  obvious from the diff.

## Branch Protection

- The `X` branch is **protected**: never commit to it directly and never
  push to it. Keep the local `X` branch in sync with `origin/X` only.
- All work happens on unprotected branches (`dev` or per-feature branches
  created from `X`). When the work is ready, open a pull request from the
  unprotected branch into `X`; merges into `X` go through PRs only.

## Agent Workflow Preferences (repository owner)

- **Do not launch the built application** after completing a task, and **do
  not run builds on your own initiative** — build only when the user
  explicitly asks for it. Verify C/C++ changes by review unless asked to
  build, and say plainly what was and was not verified.
- **Avoid routine test reruns:** review small UI edits directly; run targeted tests
  only when behavior changes, a failure needs diagnosis, or the user requests them.
- **Push only when explicitly asked** — commits stay local otherwise.
- Do not leave scratch files in the repo root; use a gitignored `build*/`
  directory and delete them when done.
