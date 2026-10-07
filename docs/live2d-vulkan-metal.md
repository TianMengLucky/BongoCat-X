# Live2D Vulkan / Metal 集成进度

## 当前代码路径

| 平台 | 默认 | 设置页可选原生后端 | 原生构建开关 | 资源 |
| --- | --- | --- | --- | --- |
| Windows | OpenGL | Vulkan | `BONGO_CAT_CUBISM_VULKAN=ON` | `.spv` |
| Linux x64 | OpenGL | Vulkan | `BONGO_CAT_CUBISM_VULKAN=ON` | `.spv` |
| macOS x64 / arm64 | OpenGL | Metal | `BONGO_CAT_CUBISM_METAL=ON` | `.metallib` |
| 其他 Linux 架构 | OpenGL | 无 | 原生后端关闭 | 无原生着色器 |

设置 → 应用 → 渲染后端在下一次帧边界生效，无需重启。切换先保存当前模型和已选动作 / 表情，释放旧渲染资源，再建立新窗口、设备和 Live2D 实例。托盘、输入和设置窗口继续使用；失败时恢复 OpenGL。设备连续三帧失败也触发回退，交换链短暂失效可重试。

SDK 可选。没有 SDK 的诊断构建仍能编译；设置页隐藏未编入的 Live2D 原生后端。运行时 Core 构建仍由用户导入 Core 库，原生后端不会改变这一许可和分发方式。

## 已接通

- 模型加载按后端选择纹理上传和 Cubism 渲染器，原生路径不要求 OpenGL 上下文。
- 图像解码经过 `bongo-safe`，保留 alpha 命中数据，应用加载时的画质 / 显示尺寸限制，再上传预乘 RGBA。
- Vulkan 通过 volk 调用系统 loader，要求 Vulkan 1.3 的动态渲染和同步特性；模型持有纹理，交换链重建时先按旧缓冲数量销毁 SDK 渲染器，再绑定新设置。
- Vulkan 在取得交换链图像后清屏并调用 Live2D，等待绘制完成，复制真实像素后转入 PRESENT 布局；Windows 分层窗口使用 BGRA 回读，Linux 使用窗口交换链和输入区域。
- Metal 从命令队列取得 command buffer，向 SDK 提供借用的 render pass / 目标纹理；SDK 完成编码，RHI 复制像素、提交和呈现。ARC 管理设备对象，模型持有原生纹理。
- 原生 GPU 回读统一转换为底部原点，供封面、逐像素点击和 Windows 分层呈现使用。
- SDK 工厂路由合并到原有用户模型安全补丁；SDK 修改只生成在构建目录，不写入仓库。

## 构建和资源

Windows / Linux x64 的 SDK 构建默认启用 Vulkan，需要 `glslangValidator` 或 `glslang` 可执行文件。Linux 可安装 `glslang-tools`。Windows 可使用 [Khronos glslang 发布包](https://github.com/KhronosGroup/glslang/releases) 或 Vulkan SDK 的 `Bin` 目录；CMake 会查找 PATH、`VULKAN_SDK` 和 `VK_SDK_PATH`。只需要编译工具和头文件，应用不链接 Vulkan SDK 导入库。

macOS 的 SDK 构建默认启用 Metal，需要 Xcode 的 `xcrun -sdk macosx metal` 和 `metallib`。对应开关设为 `OFF` 可生成 OpenGL 版本。

基础着色器及 79 种色彩 / alpha 混合组合在构建时编译。Windows 嵌入资产包包含 SPIR-V；macOS app 资产包含 metallib；Linux 安装到 `assets/FrameworkShaders`。着色器通过应用的资源根加载，不依赖启动工作目录。Metal 构建不拉取 Vulkan-Headers / volk，也不编译 Vulkan RHI 实现；Vulkan 构建不包含 Metal 实现或 metallib。资源包不包含编译中间产物或 Cubism Core。

GitHub Actions 的 release matrix 按平台选择原生后端，Linux 安装 glslang-tools，Windows 下载带 SHA-256 校验的 Khronos glslang，macOS 使用 Xcode。打包前检查对应原生着色器，CI 检查源文件平台选择和资源加载补丁；许可 / 发布保护保持生效。

## 实验性限制

- 2D 覆盖层、指针覆盖层和圆角遮罩尚在 OpenGL 路径；需要这些功能时选择 OpenGL。
- 原生后端在加载时计算纹理尺寸，尚未使用 OpenGL 的异步动态纹理刷新 / 共享纹理缓存。
- 原生帧目前同步等待 GPU、保留一帧 CPU 像素，优先保证透明呈现和命中一致性；异步 readback 和多帧流水线仍可继续优化。
- Linux 的透明 Vulkan 表面依赖窗口系统 / 驱动，macOS Metal、Windows 分层透明以及 Win32 句柄路径需要实际设备验证。

## 本轮验证范围

完成源码审阅、CMake 补丁 / 平台裁剪检查、文件行数策略、cppcheck、JSON / 工作流格式检查和发布保护自检。新增 `rhi-pixel-layout` 测试覆盖 RGBA/BGRA、行间距、底部原点、边界和透明像素。

遵循仓库“不自行构建或启动应用”的要求，本轮没有编译应用、执行 C/C++ 测试或进行 GPU 实测。后续获准构建后应验证：

1. Windows x64 / x86、Linux x64、macOS x64 / arm64 的 SDK / 诊断构建和已有测试。
2. 同一模型反复热切换，检查动作 / 表情、缩放、位置、透明度和设置窗口。
3. 蒙版 / 混合模型、镜像 / 垂直翻转、透明背景、封面和逐像素点击。
4. 连续缩放、隐藏 / 恢复、交换链变化、资源缺失及设备失败的回退。
5. 安装包和便携包从不同工作目录运行，仅携带对应平台的资源。
