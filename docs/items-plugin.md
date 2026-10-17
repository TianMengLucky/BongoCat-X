# 物品插件 / Items plugin

职责边界及新宿主要求的 companion ABI 见 [附加插件职责边界](plugin-boundaries.md)。
宿主与随附插件需一起更新；业务实现不在宿主回退。
See [add-on plugin boundaries](plugin-boundaries.md) for ownership and required companion
ABIs. Update the host and bundled plugins together; no host business fallback is used.


页面正文、交互及 UI 状态由 Rust 插件的 `src/page_ui.rs` 创建；宿主经
`include/bongo_cat/plugin_ui_page.h` 提供通用控件、绘图和业务服务。
插件内嵌自身 `locales/*.json` 的十种语言，仅解析当前语言；翻译内存由插件分配并释放。
语言加载接口为 `bongo_cat_items_locale_query_v1`（`load/text/visit/release`），
宿主 `bongo_cat_i18n_get` 转发查询，不缓存插件字符串。旧插件缺少页面扩展时需更新。

The Rust plugin owns page layout, interactions, UI state and its ten embedded locale files.
The host supplies generic drawing and application services. The versioned locale interface
loads the active language, lends strings for drawing/font baking, and releases allocations
before unload; the host never caches plugin translation pointers. Older plugins need the
page extension to display settings.

参考 [VTube Studio Item System](https://github.com/DenchiSoft/VTubeStudio/wiki/Item-System)，
为桌宠添加图片装饰。独立动态库 `bongo_items` 通过版本 1 ABI 注册「物品」页面，
不依赖 Cubism SDK，不修改模型渲染插件的 ABI。实现为 `src/rust/bongo-items/`
下的 Rust cdylib crate（`[lib] name = "bongo_items"`），导出 `bongo_cat_items_query`。

## 使用

1. 在「设置 → 插件」启用「物品」。默认构建附带该插件。
2. 打开「物品」，点击「添加物品」选择一张或多张图片，或将图片拖入页面。
   支持 PNG、JPEG、WebP、BMP 和 GIF；GIF 仅显示首帧。
3. 选中场景中的物品，在定位面板拖动标记，或调整水平／垂直位置、大小、旋转、
   不透明度、层级。可隐藏、锁定位置、复制或从场景移除。
4. 点击「保存场景」。图片导入后立即保存；其余修改在手动保存、停用插件和正常退出时保存。
   未保存期间异常退出会丢失这些修改。

最多 16 个物品，可重复使用同一图片。位置范围为 0–100%，原点在左上角；
大小为视口短边的 1–200%，保持宽高比。旋转范围为 ±180°，不透明度为 0–100%。
层级范围为 -30–30：负数在模型后方，0 及正数在前方；同层物品按添加顺序绘制。

「跟随模型视口」按模型画布定位，并跟随模型镜像及上下翻转；关闭后按整个桌宠窗口定位。
**这是画布跟随，不是 VTube Studio 的 ArtMesh 网格锚定**，不会跟随面部、骨骼或网格变形。
物品在当前桌宠窗口内绘制；超出窗口边缘（包括紧致窗口裁剪区域）的部分会被裁掉。
当前只作用于主桌宠，不在多个子进程间共享写入。尚不支持动画图片、Live2D 物品、
网格锚定、层间插入、物品热键或多场景切换。

图片存入 `<data>/items/<sha256>.img`，实例和参数存入 `<data>/items/scene.json`。
保存使用临时文件及持久替换；损坏的场景会阻止物品编辑，保留原文件供恢复。
移除实例不删除图片；停用或卸载插件保留整个物品目录。重新启用会恢复上次保存的场景。
模型封面截图不包含物品。OpenGL 直接绘制四边形；Vulkan／Metal 复用宿主的缓存背景／前景层，
呈现及像素命中测试使用同一合成结果。图片缓存每张最长边最多 1024 像素。
只有可见且不透明度大于 0 的实例保留图片缓存；隐藏、完全透明或移除最后一个使用者后回收，
恢复显示时按需重载。同一图片的可见副本继续共用缓存，移动或删除其他物品不触发整场重载。

## 构建与插件接口

- `BONGO_CAT_BUILD_ITEMS=ON` 默认构建；`OFF` 不附带动态库，宿主仍保留加载能力。
- 可在「插件」页面导入本平台动态库或仅包含一个插件的 ZIP。
  文件名：Windows `bongo_items.dll`，macOS `libbongo_items.dylib`，Linux `libbongo_items.so`。
- 查找路径沿用渲染插件：显式 `BONGO_CAT_PLUGIN_DIRECTORY`、`<data>/plugins`、
  可执行文件旁的 `plugins`、macOS `Contents/PlugIns`。停用标记在数据目录的 `plugins` 中。
- ABI：[`items_plugin.h`](../include/bongo_cat/items_plugin.h)。实例拥有场景、校验和增删改接口；
  宿主负责 Rust JSON／图片解析、导入、安全落盘和各渲染后端；插件负责页面 UI。
- 插件状态调用由实例状态锁串行保护；图片导入与场景保存使用独立工作线程。
  插件分配的实例由插件自身释放，无 SDL 对象、
  C 运行库分配或图形句柄跨越 ABI。任务进行期间不能停用／卸载；退出先排空并等待独立工作线程，再撤销回调和页面、销毁实例、卸载动态库。
- CTest `items-rust` 覆盖 ABI、路径及非有限数拒绝、容量、失败恢复原子性、删除、
  场景往返和实例隔离；`items-memory` 覆盖隐藏／透明、可见副本共享、空场景及清理失败保留。
  `items-rust` 运行 Rust crate 的单元测试，`items-memory` 需用户明确要求构建后生成的测试程序。

## English

The optional `bongo_items` dynamic plugin contributes an Items page while active. It is a Rust
cdylib crate under `src/rust/bongo-items/` (`[lib] name = "bongo_items"`) exporting
`bongo_cat_items_query`.
Enable it on Plugins, then add or drop PNG/JPEG/WebP/BMP/GIF images, including multiple files.
GIF displays its first frame. Manage up to 16 instances, including duplicates, visibility,
placement locks, position, aspect-preserving scale, rotation, opacity and layers.
Negative layers sit behind the model; zero and positive layers sit in front.
Equal layers retain insertion order. The positioning pad and numeric controls update the pet live.

Follow model viewport uses the model canvas and follows horizontal/vertical flips.
It does not pin to an animated ArtMesh. Props are clipped to the existing pet window,
including tight-frame crops. Only the primary pet owns this scene. Animated images,
Live2D props, mesh pinning, between-mesh layers, item hotkeys and multiple scenes are not implemented.

Images are copied to `<data>/items/<sha256>.img`; scene instances live in `scene.json`.
Imports save immediately. Other edits save with Save scene, plugin disable or normal shutdown;
abrupt exit loses unsaved edits. Atomic durable replacement preserves the previous saved file
on write failure. Invalid scenes remain untouched and prevent editing until repaired and re-enabled.
Removing instances preserves source images; disabling/uninstalling preserves all scene data.
Model cover captures exclude props. GL draws quads; Vulkan and Metal reuse cached background
and foreground composition. Image rendering caches fit within 1024 pixels per edge.
Caches are retained only while a visible instance with nonzero opacity uses the image.
Hiding, making fully transparent or removing its last visible user releases the cache;
showing it again reloads on demand. Visible duplicates keep sharing the same image,
and moving or removing other items preserves their caches.

`BONGO_CAT_BUILD_ITEMS=ON` builds and stages the library by default; `OFF` keeps host loading
support without bundling it. The cdylib is produced from `src/rust/bongo-items/` and keeps the
`bongo_items.dll`/`libbongo_items.so` filenames. Import a platform library or a ZIP containing one plugin on Plugins.
Resolution uses `BONGO_CAT_PLUGIN_DIRECTORY`, user plugins, bundled plugins and macOS PlugIns.
The v1 ABI keeps scene ownership and validation in the plugin; the host reuses bongo-safe
for all JSON/image decoding. State calls are serialized by an instance mutex; imports and saves run on a dedicated worker.
Allocator ownership stays within each binary,
and callbacks/page registration are removed before destruction and unload.
The `items-rust` CTest covers malformed records, bounded capacity, atomic rejected restores,
round trips and instance isolation by running the Rust crate's unit tests; the `items-memory`
CTest covers hidden/transparent images, shared visible duplicates, empty scenes and cleanup
failure retention.
