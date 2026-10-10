# Live2D Vulkan / Metal 集成进度

## 当前代码路径

| 平台 | 默认 | 设置页可选原生后端 | 原生构建开关 | 资源 |
| --- | --- | --- | --- | --- |
| Windows | OpenGL | Vulkan | `BONGO_CAT_CUBISM_VULKAN=ON` | `.spv` |
| Linux x64 | OpenGL | Vulkan | `BONGO_CAT_CUBISM_VULKAN=ON` | `.spv` |
| macOS x64 / arm64 | OpenGL | Metal | `BONGO_CAT_CUBISM_METAL=ON` | `.metallib` |
| 其他 Linux 架构 | OpenGL | 无 | 原生后端关闭 | 无原生着色器 |

设置 → 应用 → 渲染后端在下一次帧边界生效，无需重启。切换先保存当前模型和已选动作 / 表情，释放旧渲染资源，再建立新窗口、设备和 Live2D 实例。托盘、输入和设置窗口继续使用；失败时恢复 OpenGL。设备连续三帧失败也触发回退，交换链短暂失效可重试。

SDK 可选。没有 SDK 的宿主仍能构建并加载独立渲染插件；缺少 Live2D 插件或用户 Core 时使用诊断后端。运行时 Core 构建仍由用户导入 Core 库，原生后端不会改变这一许可和分发方式。

## 已接通

- 模型加载按后端选择纹理上传和 Cubism 渲染器，原生路径不要求 OpenGL 上下文。
- 图像解码经过 `bongo-safe`，保留 alpha 命中数据，应用加载时的画质 / 显示尺寸限制，再上传预乘 RGBA。
- Vulkan 通过 volk 调用系统 loader，要求 Vulkan 1.3 的动态渲染和同步特性；模型持有纹理，交换链重建时先按旧缓冲数量销毁 SDK 渲染器，再绑定新设置。
- Vulkan 在取得交换链图像后清屏并调用 Live2D，等待绘制完成，复制真实像素后转入 PRESENT 布局；Windows 分层窗口使用 BGRA 回读，Linux 使用窗口交换链和输入区域。
- Metal 从命令队列取得 command buffer，向 SDK 提供借用的 render pass / 目标纹理；SDK 完成编码，RHI 复制像素、提交和呈现。ARC 管理设备对象，模型持有原生纹理。
- 原生 GPU 回读统一转换为底部原点，供封面、逐像素点击和 Windows 分层呈现使用。
- SDK 工厂路由合并到原有用户模型安全补丁；SDK 修改只生成在构建目录，不写入仓库。

- 静态桌面背景经过 Rust 解码、SDL 缩放与镜像，按视口缓存并上传；Vulkan / Metal 每帧只在 GPU 内复制缓存背景，再绘制模型。紧贴边缘同时保留桌面背景。Windows Vulkan 实际画面检查通过，Metal 尚待真机验证。

## 构建和资源

Windows / Linux x64 的 SDK 构建默认启用 Vulkan，需要 `glslangValidator` 或 `glslang` 可执行文件。Linux 可安装 `glslang-tools`。Windows 可使用 [Khronos glslang 发布包](https://github.com/KhronosGroup/glslang/releases) 或 Vulkan SDK 的 `Bin` 目录；CMake 会查找 PATH、`VULKAN_SDK` 和 `VK_SDK_PATH`。只需要编译工具和头文件，应用不链接 Vulkan SDK 导入库。

macOS 的 SDK 构建默认启用 Metal，需要 Xcode 的 `xcrun -sdk macosx metal` 和 `metallib`。对应开关设为 `OFF` 可生成 OpenGL 版本。

基础着色器及 79 种色彩 / alpha 混合组合在构建时编译。OpenGL 源码、Vulkan SPIR-V 和 Metal metallib 均嵌入对应平台的 Live2D 动态插件。着色器通过插件内置资源加载，不依赖启动工作目录。Metal 构建不拉取 Vulkan-Headers / volk，也不编译 Vulkan RHI 实现；Vulkan 构建不包含 Metal 实现或 metallib。资源包不包含编译中间产物或 Cubism Core。

GitHub Actions 的 release matrix 按平台选择原生后端，Linux 安装 glslang-tools，Windows 下载带 SHA-256 校验的 Khronos glslang，macOS 使用 Xcode。打包前检查对应原生着色器，CI 检查源文件平台选择和资源加载补丁；许可 / 发布保护保持生效。

## 大型渲染优化

设置 → 应用 →「大型渲染优化」默认关闭，并保存到设置文件的
`application.largeRenderOptimization`。开关切换后在安全帧边界重建渲染器与模型，
保留当前选择及动作 / 表情；普通模式的命令缓冲复用和回读优化始终有效。

该开关对 Live2D 与 Inochi2D 使用同一请求入口。宿主请求四项优化，插件只返回
实际实现并启用的部分；设置页面显示实际状态。插件缺少可选入口或不支持当前后端时，
保留开关偏好并提示当前没有可用优化。不能把硬件支持某项特性视为渲染器已实现它。

| 项目 | 当前内置插件实现 |
| --- | --- |
| 多线程命令录制 | Inox2D Vulkan：至少 128 条命令时最多 4 个线程，独立命令池，保持绘制顺序；Live2D 尚未实现 |
| Bindless | 尚未实现 |
| 异步计算 | 尚未实现 |
| 大块内存分配 | Inox2D Vulkan：普通模式 GPU / 主机块为 16 / 8 MiB，开启为 64 / 16 MiB；Live2D Vulkan：VMA，优先使用 64 MiB 块 |

Inox2D 分配策略复用 `gpu-allocator`，Live2D 使用 FetchContent 获取的 VMA 3.3.0，
通过构建时生成的 SDK 适配源覆盖缓冲与图像分配、映射和释放；不修改 SDK 原文件。
VMA 分配大小是块策略，并非每个资源独占 64 MiB；对齐、专用分配与子分配由库处理。
`BONGO_CAT_FETCH_DEPS=OFF` 时须提供 `BONGO_CAT_VMA_INCLUDE_DIR`。
更大的池可能增加峰值内存，尚未实测性能收益。OpenGL / Metal 暂无这些实现。

### 其余功能的接入条件

- **Live2D 多线程录制**：Cubism 当前录制会修改遮罩上下文、共享渲染目标与 UBO。
  不能并发调用 `DrawMeshVulkan`；需要先生成不可变绘制快照，再使用独立命令池录制
  secondary command buffers，并保持动态渲染、遮罩及混合顺序。
- **Bindless**：两套后端目前均使用单纹理着色器和逐绘制描述符接口。须在创建设备时
  协商并启用所需索引特性、添加纹理表与索引着色器、处理纹理刷新和离屏目标生命周期。
  仅查询硬件支持或复用描述符并不等于 Bindless。Cubism 已缓存描述符更新。
- **异步计算**：目前两套原生后端都没有计算管线，模型更新与顶点变换由 CPU 完成。
  需要先引入适合 GPU 的变形或独立后处理任务，再管理队列、信号量、所有权及资源依赖。
  同一帧的计算结果必须在绘制前就绪；不能通过沿用上一帧结果暗中增加交互延迟。

上述未实现功能不会返回启用位。并行录制按场景大小选择执行路径，状态表示已启用策略；
实际性能与内存收益需要单独测量。此次变更只做静态检查与生成补丁匹配检查，未构建或运行。

可选插件入口为 `bongo_cat_model_plugin_optimize_v1(instance, requested, backend)`，
在 `set_rhi_info` 之后、模型加载之前调用。返回值仅包含已启用的请求位。
既有插件描述符、宿主函数表和 ABI v1 保持不变，旧插件仍可加载。

## 实验性限制

- 所有后端支持静态桌面背景和 GPU 圆角遮罩；仅绘制四个角，不新增整帧中间纹理。按键 / 效果 / 指针覆盖层仍需 OpenGL。
- 原生后端在加载时计算纹理尺寸，尚未使用 OpenGL 的异步动态纹理刷新 / 共享纹理缓存。
- 原生帧目前同步等待 GPU，优先保证透明呈现和命中一致性。Vulkan 复用命令缓冲，按提交等待 fence，持久映射回读缓冲，不再额外复制一帧 CPU 像素；圆角与回读合并提交，窗口缩放时复用圆角管线；Metal 回读缓冲随窗口缩小释放多余容量。异步 readback 和多帧流水线仍可继续优化。
- Linux 的透明 Vulkan 表面依赖窗口系统 / 驱动，macOS Metal 和 Linux Vulkan 仍待实际设备验证；Windows 分层透明已完成 GPU 回归测试。

## 本轮验证范围

按用户要求构建应用并进行测试。Windows x64 的完整 SDK 构建与无 SDK 宿主构建通过，C/C++ 使用警告视为错误。原生 Inox2D OpenGL/Vulkan 对比覆盖遮罩、七种混合模式、染色与合成；插件 ABI、模型导入、Rust JSON、窗口边界和 Live2D 资源生命周期均有回归检查。

Metal Rust 代码完成 `aarch64-apple-darwin` 类型检查，Naga 的 MSL 生成测试通过；没有 macOS SDK 链接和真实 GPU 测试，不能据此认定 Metal 真机行为通过。Linux 与 Windows x86 也尚未本地验证。

后续平台验证应覆盖：

1. Windows x86、Linux x64、macOS x64 / arm64 的 SDK / 无 SDK 构建和现有测试。
2. 同一模型反复热切换，检查动作 / 表情、缩放、位置、透明度和设置窗口。
3. 蒙版 / 混合模型、镜像 / 垂直翻转、透明背景、封面和逐像素点击。
4. 连续缩放、隐藏 / 恢复、交换链变化、资源缺失及设备失败的回退。
5. 安装包和便携包从不同工作目录运行，仅携带对应平台的插件。
