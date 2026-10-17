# 附加插件职责边界 / Add-on plugin boundaries

本文覆盖统计、音效、物品和商店；模型渲染插件沿用自身版本化 ABI。

## 业务与宿主服务

| 插件 | 插件内执行 | 宿主提供的共享服务 |
| --- | --- | --- |
| 音效 | 绑定格式、读取与保存、资源导入、提交后的试听、按键触发与释放策略 | 输入事件、短时状态锁、SHA-256、解码、独立播放器 |
| 物品 | 场景格式、读取与保存、资源导入、可见性、稳定图层顺序、跟随模型的摆放与翻转 | 图片解码、纹理缓存、原生和 OpenGL 绘制、场景变更通知 |
| 商店 | 仓库 URL、目录获取与解析、预览与下载流程、校验结果、缓存和安装回执、已安装判断、卸载选择、名称同步 | HTTP、取消和进度、校验下载、模型安装和主线程目录操作 |
| 统计 | 采集与数据库线程、快照、导出与重置完成状态、页面、悬浮窗内容与交互 | 输入和系统采样、窗口、字体、图像缓存、通用画布、通知显示 |

宿主的 `src/runtime/*` 入口负责动态库发现、启用状态、安装／卸载、生命周期、事件和
主线程集成。插件业务通过版本化接口调用；共享服务不能反过来解释插件的文件格式或
实现插件页面。现有设置扩展缓冲区及输入／模型系统继续由宿主管理。

## 复用的插件支持层

`src/rust/bongo-plugin` 提供以下共用模块，不增加外部依赖：

- `io`：有界文档读取、同步写入和原子替换、导入资产的失败清理、C 字符串和短锁辅助。
- `transfer`：借用的网络／安装服务和现有模型安装回执布局。
- `models`：不暴露宿主目录结构的主线程查询、移除、改名服务。
- `events`：插件产生通知键、回退文本和刷新标志；宿主只显示通知。
- `overlay`：复用页面的矩形、字体和颜色角色，提供无窗口／图形句柄的画布。

原有 scene/card/page ABI 不扩展结构体。新宿主要求随附插件提供相应 v1 companion：
音效／物品的 `*_io_query_v1`，物品的 `*_render_query_v1`，商店的
`*_transfer_query_v1` 与 `*_models_query_v1`，统计的 `*_overlay_query_v1`，以及
四者的 `*_events_query_v1`。更新宿主时需一并更新这些插件；缺少必需接口的旧库在加载
校验时被拒绝，不回退到宿主中的业务实现。旧宿主仍可使用插件保留的基础接口。

## 生命周期和线程约束

- IO／网络服务表及其上下文仅在一次插件调用内借用，插件不保存表或上下文。
- 场景快照和发布使用原有状态锁；磁盘、网络、解码和播放器操作不占用该锁。
- 商店的模型目录服务只在主线程执行；名称同步事件只在工作线程空闲时发送。
- 通知字符串属于动态库，宿主在调用点立即翻译／显示，不缓存指针。
- 统计悬浮窗的内容及点击状态属于插件实例，字体或窗口重建不清除页面选择。
- 停用统计插件时先关闭悬浮窗和页面，再等待插件线程退出，最后卸载动态库。
- 退出音效／物品／商店时仍先排空并等待各自工作线程；插件调用结束后才能销毁实例、
  播放器／状态锁或卸载动态库。

## English

The four add-on plugins own their formats, import/transfer policies, scene presentation,
completion messages and overlay content. The host supplies shared OS, media, networking,
rendering and model-catalog services, and handles discovery, activation, installation,
lifecycle and event delivery. Model renderer plugins retain their existing ABI.

The shared `bongo-plugin` crate supplies bounded document/asset IO, transfer and model
service bindings, notifications and a renderer-neutral overlay canvas. No external
dependency is added. Existing scene/card/page tables remain unchanged; the new host
requires the companion v1 exports listed above. Update the host and bundled add-ons
together. Older libraries missing required companions are rejected during validation;
business logic is not restored as a host-side fallback.

Service tables and contexts are borrowed only for a call. State locks protect short
snapshot/publication operations, never disk/network/decoder work. Model catalog services
run on the main thread. Shutdown joins workers before destroying contexts or unloading
libraries. Statistics closes its overlay before retirement; overlay content and pointer
state remain plugin-owned across font/window reconstruction. Notification strings are
consumed immediately and never retained after unload.

新增定向测试覆盖场景往返、非法数据、导入失败清理、有界读取、ABI 布局、物品镜像和
悬浮窗点击边界、通知只派发一次及非阻塞轮询。遵循仓库工作流，本次未构建或执行
需要编译的测试；合并前应在获准构建后运行四个插件及共享支持库的 Cargo 测试。

Targeted tests cover scene round trips, invalid records, failed-import cleanup, bounded
reads, ABI layouts, mirrored placement, overlay hit bounds and nonblocking one-shot
notifications. Builds and compiled tests were not run under the repository workflow;
run the add-on and shared-support Cargo tests when a build is authorized.
