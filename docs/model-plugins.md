# Renderer plugins

The host dispatches a versioned C ABI from `include/bongo_cat/model_plugin.h`.
ABI version 1 checks descriptor sizes, engine, required callbacks and supported
GPU backends before creating an instance. Jobs, models and GPU resources are
released before unloading a library. Cubism Framework is linked only into the
C++ Live2D plugin. There are no wrappers for the previous renderer API.

Preferences → Plugins accepts a shared library or a dragged ZIP containing exactly
one renderer library for the current platform, including nested folders. It saves
activation state and removes user-installed copies. Active plugins cannot be changed;
click selected models to deselect them or select another renderer first. Preferences → Models
allows no selection, and preserves that choice across restarts.
Bundled copies remain package-owned.
The loader checks `<data>/plugins`, then the executable's `plugins` folder;
macOS bundles additionally use `Contents/PlugIns`. `BONGO_CAT_PLUGIN_DIRECTORY`
is an explicit search override. User activation markers still apply to it.

| Engine | Windows | Linux | macOS |
| --- | --- | --- | --- |
| Live2D | `bongo_live2d.dll` | `libbongo_live2d.so` | `libbongo_live2d.dylib` |
| Inox2D | `bongo_inox2d.dll` | `libbongo_inox2d.so` | `libbongo_inox2d.dylib` |

Live2D requires user-provided Cubism Core, loaded externally through host callbacks.
Core binaries, static libraries and the SDK are never embedded in the plugin or
distributed in official packages. Static Core linking is rejected at configuration time.
User-imported plugin ZIPs may contain Core. If a usable Core is already loaded it
is retained; otherwise the existing native SDK importer selects the platform Core
from the ZIP and installs it externally in the `live2d` folders. An invalid Core
reports an import error; the renderer remains installed. Unrelated SDK files never
enter the managed plugin folder.
OpenGL shader sources and Vulkan/Metal shader binaries are embedded in the Live2D plugin. A host built
without the SDK can therefore load the plugin without a separate shader pack.

Windows portable releases are ZIP archives containing the executable and optional
`plugins` directory. Extract the entire archive; keep the plugins with the executable.

The SDK is needed to build the Live2D plugin, not to build the host.
`BONGO_CAT_REQUIRE_CUBISM=ON` requires it at configuration time.
`BONGO_CAT_BUILD_INOX2D=OFF` omits building the Rust renderer.
For offline Inox2D configuration set `BONGO_CAT_INOX2D_SOURCE` to a checkout of
`Inochi2D/inox2d` at `d4dd9dd7f16b775042cbda44370570abf9a8cf81`, and cache Cargo dependencies.
CMake prepares the pinned upstream OpenGL crate in the build tree, adding resource
ownership and host framebuffer/projection integration. Third-party source is not
vendored. Build/test the generated renderer manifest through this CMake directory;
its local upstream patch is required.

OpenGL uses upstream `inox2d-opengl`. Vulkan uses `ash` with `gpu-allocator`;
Metal uses the `metal` crate. Naga translates shared WGSL to SPIR-V/MSL. Both native
backends borrow the host device and frame target. Model textures upload at load;
per-frame vertex/parameter updates, masks and composites stay in the GPU pipeline.
There is no extra whole-frame CPU readback/upload between the plugin and host.
The host retains readback needed for layered presentation, cover capture and hit
testing. Windows transparent Vulkan presentation still requires pixel readback.

Import detects content before copying a model. An INP/INX container is validated
in Rust with bounded file/JSON/texture sizes and mesh/node checks, then parsed by
Inox2D. Supported nodes are Node, Part, Composite and SimplePhysics. The pinned
upstream does not implement MeshGroup, authored animations, opacity parameter
bindings, nested composites, masked composite children or composite mask sources;
import rejects these structures with an explanation. PNG/TGA textures are supported;
BC7 is rejected. Cubism motions and expressions are engine-specific.

Inochi2D parameter names are authored freely. The renderer accepts `Name`, `Name.x`
and `Name.y`. An optional `bongocat.bindings.json` in the model folder maps BongoCat
input parameter IDs to authored names and components; external ranges map linearly
to model ranges. Example:

```json
{
  "ParamAngleX": {"parameter": "Head", "axis": "x", "min": -30, "max": 30},
  "ParamAngleY": {"parameter": "Head", "axis": "y", "min": -30, "max": 30}
}
```

All JSON parsing/serialization, including locale files, imported metadata, Cubism
settings and comment-preserving Mver edits, uses Rust libraries (`serde_json`,
`json5`, `jsonc-parser`). The C DOM is an ownership-safe FFI adapter; files and
atomic replacement stay with the host. Allocations return through their owning
allocator, including strings returned by Rust.

## Development builds

The release workflow publishes a GitHub prerelease when a push to `dev` changes
code, assets or build configuration. Pushes changing only Markdown, `docs/` or
`LICENSE` are skipped. There is no schedule or manual nightly trigger.
Each run pins the pushed commit across all five platform builds and publishes a
unique `nightly-dev-<UTC date>-<run ID>` GitHub prerelease. Package filenames keep
the source project's version; the release notes record the exact commit.
Nightlies do not replace the latest stable release. The SDK/repository publishing
guards also apply to nightlies. Version tag pushes keep the stable release flow;
GitHub does not apply push path filters to tags. `dev` pushes also run normal CI.

Generated PNG covers are reused across selection changes and restarts. Missing,
unreadable covers or a newer model descriptor/container schedule a new capture;
capture and PNG encoding happen after selection completes. Imported models use
their existing adapter cache identity. Native Inox2D SPIR-V and MSL shaders are
translated with Naga at build time and embedded in the plugin.
