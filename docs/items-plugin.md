# 物品插件 / Items plugin

参考 [VTube Studio Item System](https://github.com/DenchiSoft/VTubeStudio/wiki/Item-System)，
为桌宠添加图片装饰。独立动态库 `bongo_items` 通过版本 1 ABI 注册「物品」页面，
不依赖 Cubism SDK，不修改模型渲染插件的 ABI。

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

## 构建与插件接口

- `BONGO_CAT_BUILD_ITEMS=ON` 默认构建；`OFF` 不附带动态库，宿主仍保留加载能力。
- 可在「插件」页面导入本平台动态库或仅包含一个插件的 ZIP。
  文件名：Windows `bongo_items.dll`，macOS `libbongo_items.dylib`，Linux `libbongo_items.so`。
- 查找路径沿用渲染插件：显式 `BONGO_CAT_PLUGIN_DIRECTORY`、`<data>/plugins`、
  可执行文件旁的 `plugins`、macOS `Contents/PlugIns`。停用标记在数据目录的 `plugins` 中。
- ABI：[`items_plugin.h`](../include/bongo_cat/items_plugin.h)。实例拥有场景、校验和增删改接口；
  宿主负责 Rust JSON／图片解析、导入、安全落盘、UI 和各渲染后端。
- 插件状态调用由实例状态锁串行保护；图片导入与场景保存使用独立工作线程。
  插件分配的实例由插件自身释放，无 SDL 对象、
  C 运行库分配或图形句柄跨越 ABI。任务进行期间不能停用／卸载；退出先排空并等待独立工作线程，再撤销回调和页面、销毁实例、卸载动态库。
- CTest `items-plugin` 覆盖 ABI、路径及非有限数拒绝、容量、失败恢复原子性、删除、
  场景往返和实例隔离。运行该测试需要用户明确要求构建后生成的测试程序。

## English

The optional `bongo_items` dynamic plugin contributes an Items page while active.
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

`BONGO_CAT_BUILD_ITEMS=ON` builds and stages the library by default; `OFF` keeps host loading
support without bundling it. Import a platform library or a ZIP containing one plugin on Plugins.
Resolution uses `BONGO_CAT_PLUGIN_DIRECTORY`, user plugins, bundled plugins and macOS PlugIns.
The v1 ABI keeps scene ownership and validation in the plugin; the host reuses bongo-safe
for all JSON/image decoding. State calls are serialized by an instance mutex; imports and saves run on a dedicated worker.
Allocator ownership stays within each binary,
and callbacks/page registration are removed before destruction and unload.
The `items-plugin` CTest covers malformed records, bounded capacity, atomic rejected restores,
round trips and instance isolation.
