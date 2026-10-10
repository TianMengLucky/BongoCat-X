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
- Vulkan 在取得交换链图像后初始化背景并调用 Live2D：有完整缓存背景时直接复制，无背景时清屏；背景复制合并到初始化提交，省去重复清屏和单独提交。等待绘制完成，复制真实像素后转入 PRESENT 布局；Windows 分层窗口使用 BGRA 回读，Linux 使用窗口交换链和输入区域。
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

设置 → 应用 →「大型渲染优化」在当前实际后端为 Vulkan / Metal 时显示，默认关闭，并保存到设置文件的
`application.largeRenderOptimization`。开关切换后在安全帧边界重建渲染器与模型，
保留当前选择及动作 / 表情；普通模式的命令缓冲复用和回读优化始终有效。

该开关对 Live2D 与 Inochi2D 使用同一请求入口。Vulkan 展开后三个子开关可独立保存，Metal 仅显示大块内存分配；旧设置缺少子项时默认全部选中，保留原先行为。关闭总开关或切到其他后端不会清除子项偏好，OpenGL 不发送优化请求。宿主仅请求选中的优化，插件只返回
实际实现并启用的部分；设置页面显示实际状态。插件缺少可选入口或不支持当前后端时，
保留开关偏好并提示当前没有可用优化。不能把硬件支持某项特性视为渲染器已实现它。

| 项目 | 当前内置插件实现 |
| --- | --- |
| 多线程命令录制 | Inox2D Vulkan：至少 128 条命令时最多 4 个线程，独立命令池，保持绘制顺序；Live2D Vulkan：普通混合、无离屏节点、非高精度遮罩模型，至少 128 个 Drawable 进入次级缓冲路径，至少 128 条可绘制快照使用最多 4 个线程 |
| Bindless | Live2D Vulkan：最多 64 张图集的纹理表，普通混合且无离屏节点；所需设备特性或容量不足时回退。Inox2D 尚未实现 |
| 大块内存分配 | Inox2D Vulkan：普通模式 GPU / 主机块为 16 / 8 MiB，开启为 64 / 16 MiB；Inox2D Metal：通过大块内存分配开关启用整帧共享顶点缓冲；Live2D Vulkan：VMA，首选 16 MiB 块，持久资源采用最小内存策略，纹理上传缓冲独立分配并在同步上传后释放 |

Inox2D Vulkan 分配策略复用 `gpu-allocator`，Live2D 使用 FetchContent 获取的 VMA 3.3.0，
通过构建时生成的 SDK 适配源覆盖缓冲与图像分配、映射和释放；不修改 SDK 原文件。
VMA 分配大小是块策略，并非每个资源独占 64 MiB；对齐、专用分配与子分配由库处理。
`BONGO_CAT_FETCH_DEPS=OFF` 时须提供 `BONGO_CAT_VMA_INCLUDE_DIR`。
更大的池可能增加峰值内存，尚未实测性能收益。Inox2D Metal 内部已实现可选的整帧
共享顶点缓冲路径，各节点按字节偏移读取，空帧不分配；每帧使用独立缓冲，
由 Metal 命令缓冲保留至执行完成，避免覆盖仍在使用的顶点数据。
设置界面在 Vulkan 提供三个子开关，在 Metal 仅提供大块内存分配；宿主按后端
请求已选项。Inox2D Metal 启用该项时使用整帧共享缓冲，关闭后逐节点分配；
Live2D Metal 显示不可用。性能收益与 macOS 真机行为待验证。
OpenGL 不使用这些可选请求；Live2D Metal 的自动共享顶点路径不对应此可选能力位。

### 其余功能的接入条件

- **Live2D 多线程录制已接入部分场景**：普通混合、无离屏节点且未启用高精度遮罩，
  至少 128 个 Drawable 时使用动态渲染的 secondary command buffers。遮罩、顶点上传、
  描述符和 UBO 准备仍在调用线程；工作线程只读取管线、缓冲和描述符等不可变快照，
  不调用 `DrawMeshVulkan`，不访问可变 Cubism 状态。至少 128 条可绘制快照才并行，
  最多 4 个工作线程各有独立且复用的命令池，按连续区间录制，再按原顺序执行。
  实际快照不足时串行录制一个次级缓冲；其他模型保留 SDK 原路径。设备至少需要
  两个可用 CPU 线程；池初始化失败不报告启用，工作线程在初始化时创建并持续复用，启动失败时关闭并行策略。
  SDK 每次绘制提交等待 GPU 完成后才复用命令池；异常清除未完成快照，释放时等待设备。
  高级混合、离屏嵌套和高精度遮罩的并行路径尚未接入；开启状态表示策略可用，
  不保证每个模型或每帧都并行。实现依据 [Vulkan 动态渲染与次级缓冲规则](https://docs.vulkan.org/features/latest/features/proposals/VK_KHR_dynamic_rendering.html)。
- **Live2D Bindless**：由宿主实际启用 `shaderSampledImageArrayDynamicIndexing` 后，
  通过可选入口传递启用位。普通混合且无离屏节点的模型采用 set 1 的固定 64 项图集表，
  每次绘制通过 push constant 传入一致的图集索引；所有槽位均初始化，不要求非一致索引或更新后绑定。
  遮罩仍使用 SDK 描述符。模型创建时等待设备并更新纹理表，可与多线程快照录制共同使用。
  图集超过 64 张、设备限制不足、旧宿主未传递启用位或模型不适用时保留 SDK 路径。
  Inox2D Bindless 尚未接入。异步计算选项及配置字段已删除，旧配置字段被忽略。

未实现功能不会返回启用位。Bindless 与并行录制按模型条件选择执行路径，状态表示已启用策略；
实际性能与内存收益需要单独测量。本次通过 SDK 生成补丁的匹配、串行回退不变和 SDK 锚点变更拒绝检查，未构建或运行。

可选插件入口为 `bongo_cat_model_plugin_optimize_v1(instance, requested, backend)`，
在 `set_rhi_info` 之后、模型加载之前调用。返回值仅包含已启用的请求位。
既有插件描述符、宿主函数表和 ABI v1 保持不变，旧插件仍可加载。

## 实验性限制

- 所有后端支持静态桌面背景和 GPU 圆角遮罩；仅绘制四个角，不新增整帧中间纹理。按键 / 效果 / 指针覆盖层仍需 OpenGL。
- Live2D 的 OpenGL、Vulkan 和 Metal 均按窗口尺寸动态更新纹理。原生后端逐张在后台解码和缩放，再由主线程上传并替换；取消或失败时保留当前纹理，窗口调整期间合并请求，隐藏时清理过期任务。关闭动态分辨率通过模型重载恢复画质比例允许的尺寸，失败时回滚选项。原生 GPU 上传仍同步等待，不支持 OpenGL 的共享纹理缓存；Inox2D 的动态分辨率仍未接入。
- 原生帧目前同步等待 GPU，优先保证透明呈现和命中一致性。Vulkan 复用命令缓冲，按提交等待 fence，持久映射回读缓冲，不再额外复制一帧 CPU 像素；圆角与回读合并提交，窗口缩放时复用圆角管线；Metal 回读缓冲随窗口缩小释放多余容量。异步 readback 和多帧流水线仍可继续优化。
- Linux 的透明 Vulkan 表面依赖窗口系统 / 驱动，macOS Metal 和 Linux Vulkan 仍待实际设备验证；Windows 分层透明已完成 GPU 回归测试。

## 此前版本验证范围

以下结果来自此前按用户要求执行的构建，不覆盖本轮新增优化。Windows x64 的完整 SDK 构建与无 SDK 宿主构建通过，C/C++ 使用警告视为错误。原生 Inox2D OpenGL/Vulkan 对比覆盖遮罩、七种混合模式、染色与合成；插件 ABI、模型导入、Rust JSON、窗口边界和 Live2D 资源生命周期均有回归检查。

Metal Rust 代码完成 `aarch64-apple-darwin` 类型检查，Naga 的 MSL 生成测试通过；没有 macOS SDK 链接和真实 GPU 测试，不能据此认定 Metal 真机行为通过。Linux 与 Windows x86 也尚未本地验证。

后续平台验证应覆盖：

1. Windows x86、Linux x64、macOS x64 / arm64 的 SDK / 无 SDK 构建和现有测试。
2. 同一模型反复热切换，检查动作 / 表情、缩放、位置、透明度和设置窗口。
3. 蒙版 / 混合模型、镜像 / 垂直翻转、透明背景、封面和逐像素点击。
4. 连续缩放、隐藏 / 恢复、交换链变化、资源缺失及设备失败的回退。
5. 安装包和便携包从不同工作目录运行，仅携带对应平台的插件。

## 自动执行的 Live2D 优化

以下优化无需开启「大型渲染优化」，不新增设置；原有并行录制、Bindless、VMA 子项仍按用户偏好协商。

- OpenGL 将模型顶点、UV 和索引分别合并到共享缓冲，以偏移绘制；未变化的数据省去重复上传。动态流使用缓冲 orphaning 和可用时的映射写入，失败回退到子数据上传。缓存连续重复的程序、纹理与混合状态，资源删除及作用域结束时清除缓存。
- Vulkan 将整帧网格顶点合并上传，遮罩和主绘制复用；设备支持时直接写入 coherent、CPU 可见的 device-local 内存，否则使用共享 staging。初始化纹理和索引按最多 32 个命令、约 16 MiB 暂存预算分批提交，单张大图可超过预算；暂存资源在 fence 完成后释放。保留同步帧语义，复用一组 SDK 帧资源，以 fence 代替常规队列空闲等待，并将相邻信号 / 等待提交合并到一次调用。管线使用进程内 VkPipelineCache；重复的管线、描述符和剔除状态绑定被跳过。可选并行录制复用常驻工作线程。
- Metal 合并模型顶点与 UV，最多复用三个共享缓冲槽；GPU 完成后才覆盖对应槽，空模型保留原路径。静态索引避免重复复制；缓存同一 encoder 内连续重复的 PSO、纹理、剔除和绕序状态。PSO 沿用 SDK 的初始化与缓存机制。
- Vulkan / Metal 遮罩随窗口尺寸调整，带缩放迟滞，尺寸上限为 2048，并受设备纹理上限约束。调整时更新遮罩资源并使依赖描述符失效。

本轮 Windows x64 Release SDK 构建通过，应用代码启用警告视为错误；新增 Vulkan 提交测试与五项 SDK 生成补丁检查共六项通过，包括嵌入 UTF-8 BOM 的回归检查。已检查 SDK 原文件未修改、锚点变化拒绝、静态分析与行数限制。未启动应用，OpenGL GPU 回归与 Metal 构建尚未执行。CPU / GPU 时间及总内存收益需要在相同模型和设备上测量，macOS 真机行为待验证。

后续优化将 Vulkan staging 的顶点传输范围限制到变化网格覆盖的连续区间（首帧与失败恢复完整上传），移除 Metal 纹理上传的中间 RGBA 副本，并在 OpenGL 渲染器释放时回收 CPU 几何缓存容量。此后续改动未构建或运行，实际内存和 GPU 收益待测量。

Vulkan VMA 路径进一步复用已有 coherent 映射整理顶点，省去额外交错 CPU 数组与复制；非池化路径继续使用 CPU 打包缓冲。上传将相邻变化网格合并，跳过未变化间隔，源偏移分别适配映射缓冲和从零开始的 staging。顶点准备失败会使缓存失效。本轮新增区段回归用例未执行，未构建或测量性能。
