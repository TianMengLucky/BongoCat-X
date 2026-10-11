# 统计插件 / Statistics plugin

Rust crate：`src/rust/bongo-stats`，独立 `cdylib`，不与宿主静态链接。
键鼠功能参考 [keyStats](https://github.com/debugtheworldbot/keyStats)，前台应用计时参考
[Work-Review](https://github.com/wm94i/Work-Review) 的应用识别、空闲暂停与本地存储方式。
仅本地记录键鼠汇总、应用标识／名称和时长；不记录输入文本、按键名称／顺序、坐标、
窗口标题、网址或截图，不联网。

## 使用

- 默认构建并打包 `plugins/bongo_stats.dll`（Windows）、
  `plugins/libbongo_stats.so`（Linux）或 `Contents/PlugIns/libbongo_stats.dylib`（macOS）。
- 「插件」页可启用、停用、卸载或导入动态库／ZIP；ZIP 应只包含当前平台的一个插件。
  用户安装目录为 `<data>/plugins`，优先于随程序附带的插件。
  `BONGO_CAT_PLUGIN_DIRECTORY` 可覆盖发现目录，沿用渲染插件的测试约定。
- 「统计」页提供今日、近 7 天、近 30 天、累计数据，键鼠近 30 天与今日小时柱状图，
  应用时长排行及今日每小时使用时长。日历窗口包含今日，午夜自动切换。
- 键鼠指标：键盘按下次数、左／右／中／后退／前进按钮次数、移动量和滚轮量。
  长按自动重复不重复计数，松键和手柄不计入键鼠指标。
- 应用计时每秒查询前台应用，只计算单个前台应用；窗口切换按上一采样区间归属，
  误差约一个采样周期。空闲达到 5 分钟、锁屏、无法识别应用或休眠时不计时；
  超过 5 秒的采样间隔与明显时钟跳变整段跳过，恢复后重新建立基线。
  使用时长表示有输入活动的前台使用，长时间无操作阅读／看视频会被视为空闲。
- 应用采集可独立关闭，不影响键鼠计数，关闭后仍可查看历史。
- 导出异步执行，完成后显示提示。键鼠写入 `<data>/statistics-export.csv`，
  `date=0,hour=0` 是累计汇总；应用小时明细写入 `<data>/application-usage.csv`。
- 停用即停止新的输入采集，后台完成最终保存；卸载保留数据库。
  统计入口由 Rust 插件提供，启用时注册，停用／卸载时立即移除；
  正在查看统计页时会返回「插件」页。宿主只渲染已注册的入口。
  保存或导出失败在页面／通知中显示，桌宠继续运行。

## 线程与存储

统计与桌宠线程独立：宿主输入监听仅累加原子计数，Rust 工作线程每 200ms 取走汇总，
每秒识别应用并发布快照，每 30 秒增量保存。数据库打开、恢复、查询、事务和 CSV 写入
都由 Rust 工作线程执行。统计页使用非阻塞缓存读取，缓存忙时保留上一快照。
停用／卸载／更新插件通过后台清理线程等待最终保存和卸载，桌宠主循环不等待数据库。
程序退出时会等待清理完成，再销毁输入状态与卸载动态库，防止悬空回调。

数据库：`<data>/statistics/statistics.sqlite3`，SQLite WAL、FULL 同步与事务。
异常断电可能丢失最近 30 秒未保存的数据。仅主进程采集，多宠物子进程不写入数据库。
保留最近 730 个本地日历日的小时明细，累计数据长期保留；应用页面缓存最近 30 天。
最多记录 512 个应用标识，页面展示时长最多的 64 项，总时长涵盖所有已记录应用。
键鼠批次按统计线程取走时的本地时间归档，边界精度约 200ms，存储繁忙时会降低。
应用计时在本地小时／日期边界拆分；夏令时重复小时合并，不把时钟跳变记为使用时间。

## 平台与单位

- Windows：使用前台窗口的进程可执行文件名（不保存完整路径）；同名程序合并。
  系统最后输入时间用于空闲检测，输入桌面用于识别锁屏。
- macOS：使用前台应用 bundle ID 与应用名称，系统输入时间与会话状态用于暂停。
  不抓取窗口标题或屏幕图像。
- Linux：X11 使用独立 XCB 连接及 XScreenSaver 协议；构建时可选 `xcb`、
  `xcb-screensaver` 开发库，缺失时仅应用计时不可用，键鼠统计保持可用。
  Wayland 缺少通用前台应用协议，暂不支持应用计时；不产生不完整的 XWayland 排行。
  X11 使用 `WM_CLASS` 标识，屏幕保护器活动或空闲时暂停。

移动量：Windows／X11 桌面像素、macOS 桌面点、evdev 原始相对输入单位，
不是米／厘米，不能直接比较设备。Windows 按原始输入包采样光标，macOS／X11
使用现有指针监听，evdev 按 `SYN_REPORT` 累加二维移动；坐标跳变可能计入。
累计量以 1/1024 输入单位量化；并发指针更新只尝试一次，繁忙时略过该坐标采样，
不会等待统计锁。键盘和按钮使用独立原子计数，不受宠物队列满或合并影响。
滚动量累加两个轴绝对值；Windows 使用 `WHEEL_DELTA` 刻度，macOS 系统线单位，
X11／evdev 滚轮刻度。触摸板与鼠标单位可能不同，不换算物理距离。
全局输入权限沿用宿主机制，未授权时只能统计实际收到的事件。

## 开发

- `BONGO_CAT_BUILD_STATISTICS=ON` 默认启用，设为 `OFF` 不附带统计动态库。
- 复用 Corrosion、`chrono` 和 `rusqlite`（bundled SQLite），不引入 JSON 解析器。
- 独立构建：`cargo build --locked --release --manifest-path src/rust/bongo-stats/Cargo.toml`。
- 定向测试：`cargo test --locked --manifest-path src/rust/bongo-stats/Cargo.toml`；
  CTest 注册 `statistics-rust` 与 `input-metrics`。
- 基础 ABI：`include/bongo_cat/statistics_plugin.h`，版本 1；额外工作线程与应用计时 ABI：
  `include/bongo_cat/statistics_usage_plugin.h`，版本 1；页面贡献 ABI：
  `include/bongo_cat/statistics_page_plugin.h`，版本 1。宿主要求三者均兼容。
  页面入口标识、标题翻译键和图标由插件提供，描述符复制到宿主后注册；
  停用时先撤销注册，再由后台线程完成保存与卸载。
  `configure` 注册线程安全的汇总／应用查询回调，回调只在 Rust 工作线程中执行。
  `snapshot` 不等待，`export_csv` 返回任务是否入队，`export_status` 查询完成状态。
  `flush`／`destroy` 是生命周期屏障，运行中的桌宠线程不调用这两个等待接口。
  回调上下文必须一直有效到 `destroy` 结束；动态库随后才能卸载。

## English

The independent Rust dynamic plugin combines basic input counters inspired by
[keyStats](https://github.com/debugtheworldbot/keyStats) with foreground app timing
inspired by [Work-Review](https://github.com/wm94i/Work-Review). Only aggregate
input values, app IDs/names and durations stay on this device. It collects no
typed text, key identities/order, coordinates, window titles, URLs or screenshots.

The Statistics page shows today, the last 7/30 calendar days including today,
lifetime totals, input charts, app rankings and today's hourly app time.
Application recording has its own switch. One foreground app is sampled each
second. Idle time of five minutes, screen lock and unknown apps pause tracking.
Intervals over five seconds and large clock changes break the billing baseline.
Passive reading or video playback without input eventually counts as idle.
Focus changes have approximately one sample interval of uncertainty.

The navigation entry is contributed by the Rust plugin and registered only
while enabled. Disabling or removing the plugin immediately removes the entry;
an open Statistics page returns to Plugins. The host renders registered entries.
Collection is independent of the pet loop. Input listeners update atomic
counters; Rust drains them every 200ms, probes apps each second and saves every
30 seconds. Database opening, transactions, exports and recovery stay on the
worker. UI uses try-lock cache reads and retains the previous snapshot if busy.
Interactive disable/removal/replacement joins and unloads on a cleanup thread.
Only application shutdown waits for cleanup before destroying callback context.

SQLite WAL/FULL transactions store data at
`<data>/statistics/statistics.sqlite3`. Abrupt shutdown can lose the unsaved
interval. Details retain 730 local calendar days; lifetime totals remain.
Application snapshots cache 30 days, at most 512 IDs and the top 64 rankings.
Removing or disabling preserves history. Async exports create
`<data>/statistics-export.csv` and `<data>/application-usage.csv`; input date/hour
zero indicates lifetime totals. App CSV names are quoted and formula-protected.

Windows uses executable basenames, macOS uses bundle IDs/localized names, and
X11 uses WM_CLASS through a separate XCB connection. X11 app timing needs optional
xcb/xcb-screensaver development libraries. Missing libraries and Wayland sessions
leave app timing unavailable while input counters remain usable.

Movement uses desktop pixels (Windows/X11), points (macOS) or raw units (evdev),
quantized to 1/1024 input unit. Pointer producers try once and may skip a sample
under contention, never waiting for statistics. Wheel units differ by backend.
These are not physical distances. Existing global-input permissions still apply.

Corrosion builds `bongo-stats` by default; `BONGO_CAT_BUILD_STATISTICS=OFF` omits
it. The base, worker/application and page-contribution headers each declare
a required v1 ABI. The plugin supplies its navigation metadata; the host copies
and registers it for the active instance. Worker callbacks must remain valid
until joined destruction returns;
never unload the plugin earlier. The targeted Rust tests cover ranges, durable
storage, app switches, idle/lock/suspend/pause, boundaries and nonblocking caches.
Per-key heatmaps, website/screenshot tracking and multi-device sync are outside
this plugin's scope.
