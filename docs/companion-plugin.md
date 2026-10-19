# AI 陪伴插件（实验性） / AI companion (experimental)

本插件处于实验阶段，**默认构建并包含在常规发行包中**，运行时默认关闭。
仅当用户显式安装、配置并启用后才会向 AI 服务发送请求。

## 安装与配置

- `BONGO_CAT_BUILD_COMPANION` 默认值为 `ON`，构建并打包独立的
  `bongo_companion` 动态库。可传入 `-DBONGO_CAT_BUILD_COMPANION=OFF` 排除它。
  已缓存 `OFF` 的构建目录需要显式传入 `-DBONGO_CAT_BUILD_COMPANION=ON` 重新配置。
- 可在设置的插件页导入当前平台的动态库（Windows `.dll`、macOS `.dylib`、
  Linux `.so`）；首版不接收此插件的 ZIP。也可放入 `<data>/plugins` 后重启。
- 安装后打开「AI 陪伴（实验性）」页面。配置完整的 Chat Completions HTTPS 地址、
  模型、API 密钥环境变量名、桌宠人设，以及各项触发开关。
- 默认地址 `https://api.deepseek.com/chat/completions`，默认模型 `deepseek-flash`。
  两者均可修改；DeepSeek 请求关闭思考模式以便及时返回简短回复。
  参考 [DeepSeek 官方示例](https://api-docs.deepseek.com/api_samples/chat_curl/)。
- 密钥不写入配置或日志。启动程序前设置 `DEEPSEEK_API_KEY`，或改用页面指定的
  其他环境变量。PowerShell 可执行 `$env:DEEPSEEK_API_KEY = "你的密钥"`，
  再从同一终端启动 BongoCat；持久环境变量需要重新启动应用才会生效。
- 保存后启用生效；「测试连接」使用已保存配置，也遵循启用状态和冷却时间。
  调用远端服务可能产生费用。

## 触发与展示

提供启动、空闲、键盘活动、鼠标活动、桌宠行为五类触发开关。
桌宠行为覆盖统一行为入口中的动作、表情、特效及音效；不发送具体行为 ID。
鼠标包含点击和移动；键盘仅报告活动，**不采集或上传按键内容、输入文本、
应用标题、文件路径或屏幕内容**。发送的内容只有事件类别和用户设置的人设。

默认请求冷却为 60 秒（最小 10 秒），空闲阈值 300 秒，显示时间 15 秒。
同一时刻只允许一个后台请求，无排队和自动重试。每次空闲周期最多触发一次。
请求总时限为 30 秒；请求和响应各限 64 KiB，回复最多保留 512 个字符。
HTTPS 请求禁止重定向，避免密钥被带往其他地址；错误正文不进入日志或界面。
回复在桌宠左侧独立文本框自动换行显示，最多八行；超长内容以省略号结尾。
点击关闭，到期消失；跟随桌宠移动并约束在可用屏幕区域内，空间不足时可能覆盖桌宠。
桌宠隐藏、最小化或仅录制模式下不显示文本框。

配置保存在 `<data>/companion/settings.json`，使用临时文件和宿主的持久原子替换。
保存新设置会取消在途请求并丢弃旧回复。损坏配置不会被默认配置静默覆盖；
修复或移走该文件后重启。停止插件时先取消并等待后台请求退出，再卸载动态库。

## TTS 扩展接口

`include/bongo_cat/companion.h` 提供版本化 `BongoCatCompanionTts` 和
`bongo_cat_companion_set_tts(app, &provider)`。插件加载后可注册提供方；
传入 `NULL` 注销。`speak(context, utf8)` 与 `cancel(context)` 在主线程调用，
必须立即返回，语音合成及播放由提供方异步排队。文本仅在回调期间借用，
需要异步使用时由提供方复制。新回复取消旧语音，关闭或到期取消当前语音；
停止插件或注销也会取消。提供方卸载前必须注销，避免保留失效函数指针。
首版不包含语音服务、语音凭据或音频播放实现。

## 边界与验证

Rust 插件负责设置格式、事件策略、请求 JSON、响应解析及文本布局。
宿主只提供 HTTP、原子替换、插件发现和生命周期、通用设置控件、字体与跟随窗口。
统计悬浮窗也复用通用跟随窗口服务，仍保留原来的位置和交互。

新增定向 Rust 测试覆盖配置往返、输入上限、响应解析、UTF-8 截断、ABI、
默认禁用、冷却、空闲活动判定、旧响应丢弃和取消后退出。
遵循仓库要求，此次未构建、未运行需要编译的测试、未启动应用，也未实际调用付费 API。

## English

This experimental plugin is built and included in normal release packages by default,
but remains disabled at runtime. Set `-DBONGO_CAT_BUILD_COMPANION=OFF` to omit it,
or explicitly set `ON` when reconfiguring a build directory that cached `OFF`.
For a separate installation, import the platform library on the Plugins page or place it
in `<data>/plugins` and restart. ZIP installation is not supported in this first version.
Configure and explicitly enable it on the experimental companion page.

The default endpoint is `https://api.deepseek.com/chat/completions`; the editable model is
`deepseek-flash`. Set `DEEPSEEK_API_KEY` before launching the application, or choose
another environment variable in settings. Keys are never persisted or logged. The test
button uses saved settings and respects activation and cooldown; API charges may apply.

Choose startup, idle, keyboard activity, mouse activity, or pet behavior triggers. Only the
category and configured system prompt leave the device. There is no key text, application,
path, or screen capture. One request runs at a time, with no queue or automatic retry.
Defaults: 60-second cooldown, 300-second idle threshold, and 15-second display duration.
Requests time out after 30 seconds and both bodies are bounded to 64 KiB. HTTPS redirects
are rejected. Replies are limited to 512 characters and shown beside the pet on its left,
wrapped to eight lines with an ellipsis. Click to dismiss. The window follows the pet and
stays within usable screen bounds; it can overlap the pet when space is insufficient.

Settings live in `<data>/companion/settings.json`, saved through durable atomic replacement.
Changing settings cancels outstanding work and discards stale replies; shutdown joins workers
before unloading. Invalid stored settings are preserved for repair rather than overwritten.

The optional v1 TTS API in `include/bongo_cat/companion.h` registers nonblocking main-thread
`speak` and `cancel` callbacks. Copy borrowed UTF-8 text before asynchronous synthesis.
Unregister before unloading a provider. No TTS provider is bundled. Targeted tests are added;
no build, compiled test, application launch, or live API call was performed for this change.
