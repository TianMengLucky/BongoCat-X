# 统计插件 / Statistics plugin

职责边界及新宿主要求的 companion ABI 见 [附加插件职责边界](plugin-boundaries.md)。
宿主与随附插件需一起更新；业务实现不在宿主回退。
See [add-on plugin boundaries](plugin-boundaries.md) for ownership and required companion
ABIs. Update the host and bundled plugins together; no host business fallback is used.


页面正文、交互及 UI 状态由 Rust 插件的 `src/page_ui.rs` 创建；宿主经
`include/bongo_cat/plugin_ui_page.h` 提供通用控件、绘图和业务服务。
插件内嵌自身 `locales/*.json` 的十种语言，仅解析当前语言；翻译内存由插件分配并释放。
语言加载接口为 `bongo_cat_statistics_locale_query_v1`（`load/text/visit/release`），
宿主 `bongo_cat_i18n_get` 转发查询，不缓存插件字符串。旧插件缺少页面扩展时需更新。

The Rust plugin owns page layout, interactions, UI state and its ten embedded locale files.
The host supplies generic drawing and application services. The versioned locale interface
loads the active language, lends strings for drawing/font baking, and releases allocations
before unload; the host never caches plugin translation pointers. Older plugins need the
page extension to display settings.

统计页支持「重置统计」，经页面内确认后，在 Rust 工作线程用 SQLite 事务清空
输入次数、每日／小时图表及应用时长历史，并立即刷新已发布缓存。失败时回滚且保留历史；
统计开关设置、插件启用状态与已导出的 CSV 保留。重置使用可选的版本 1 扩展
`include/bongo_cat/statistics_reset_plugin.h`，旧插件仍可统计，但需更新后才能重置。
统计页分为「键鼠统计」、「应用统计」和「字数统计」。键鼠页提供概览卡片、键盘热力图及 7／30 天折线趋势；应用页按时长排列图标、时长条和时长文字。悬浮窗采用跟随明暗主题的圆角卡片，底部可切换键鼠／应用两页；应用页仅显示今日前 3 个应用。

The Statistics page offers Reset statistics with inline confirmation. The Rust worker
clears input counters, daily/hourly charts and application time in one SQLite transaction,
and publishes fresh snapshots before reporting completion. Failure rolls back and preserves
history. Collection settings, plugin activation and previously exported CSV files are kept.
Reset uses the optional version 1 extension in `include/bongo_cat/statistics_reset_plugin.h`;
older plugins still collect statistics and need updating to offer reset.
The Statistics page has Keyboard and mouse, Application statistics and Character statistics subpages. Input statistics include summary cards, a keyboard heatmap and 7/30-day line charts. Application rows show an icon, duration bar and duration text. The rounded floating card follows the light/dark theme, with bottom tabs for input statistics and today’s top three applications.


Rust crate：`src/rust/bongo-stats`，独立 `cdylib`，不与宿主静态链接。
键鼠功能参考 [keyStats](https://github.com/debugtheworldbot/keyStats)，前台应用计时参考
[Work-Review](https://github.com/wm94i/Work-Review) 的应用识别、空闲暂停与本地存储方式。
仅本地记录键鼠汇总、各键累计次数、应用标识／名称、图标和时长；新增字数功能仅保存无序汉字频次，不保存句子、按键顺序、坐标、
窗口标题、网址或截图，不联网。

## 字数与速度 / Characters and typing speed

「字数统计」页提供默认关闭的「扣除 Backspace 删除的汉字」开关，设置会保存。
开启后仅扣除当前焦点输入框内、本次采集已计入且确实通过退格删除的汉字；
修改拼音、粘贴或历史文本不会扣数。切换焦点、鼠标点击、快捷键、导航、重置或重启
都会丢弃内存中的删除跟踪。计数、字频和速度按原提交日期扣除，打字时长不回退。
此功能依赖可访问文本的前后变化，受已有上下文长度限制，无法观测到的删除可能漏扣；
输入顺序和删除跟踪不写入数据库或日志。

统计页新增第三个「字数统计」子页面，提供今日、近 7／30 天和累计汉字数及前 32 个常用汉字。
Windows 优先通过 UI Automation 的 `CompositionFinalized` 事件采集输入法最终提交。
对于未提供最终提交事件的应用，每 100 毫秒检查焦点控件的 `TextEditPattern` 组合状态，
在组合结束后读取已观察到的组合范围，并与提交事件去重；空范围和仅含空白的范围不视为组合。
同时独立检查普通 `TextPattern` 的光标前上下文，兼容 QQ 等组合状态未及时清除的控件。
兼容路径以实际键盘输入为触发，在空格、回车、数字或标点确认候选后，按新增文本计汉字，
不用按键数估算字数。输入框首次出现时只建立基线；快捷键、鼠标点击、删除、导航和取消会重新建立基线。
兼容路径暂存最多 256 个文本单位的光标前上下文（上限 4096 UTF-16 单位），
只在内存中比较，不写日志或数据库；读取范围始终限制在焦点文本控件内。
QQ 等控件可能返回文本为空但端点不重合的选区，或返回超出自身文本范围的光标；
这时兼容路径读取输入框末尾的有限上下文，较长输入框中间的编辑仍可能漏计。
鼠标选词、无键盘确认的输入、过快切换焦点，以及应用不提供文本接口时可能漏计。
订阅成功不代表当前应用支持采集。
不支持的平台、启动失败及事件丢失会在页面提示，不会用按键数估算汉字数。
只处理有键盘焦点的非密码控件，不扫描文档，不读取剪贴板。
汉字按 Unicode CJK 统一及兼容表意文字范围计数；不计标点、拉丁字母和普通粘贴，
无法仅凭字符区分中文与日文等语言中的汉字。

最近一分钟速度为过去 60 秒提交的汉字数（字／分钟）。平均速度仅针对本次运行：
汉字数除以有效输入分钟，每段输入起计 1 秒；两次提交间隔不超过 5 秒时计入间隔，
超过 5 秒则另起一段。两种速度均不受所选历史时段影响，重启及重置后重新计算。
这是基于提交时间的估算，不包含首次提交前的拼音输入时间。

仅本地保存每日／累计无序字频；事件文本在回调内转成频次后释放，兼容路径的上下文只保留在内存中。
最近 30 天每日字频及累计字频在现有 SQLite 事务内保存，沿用 30 秒保存间隔。
「重置统计」同时事务清除字频及速度；现有 CSV 导出内容保持原有键鼠／应用数据。
停用插件后取消订阅，等待回调释放后再卸载；不受支持的应用不会产生字数。

The Characters page includes a saved, default-off option to subtract Han characters
removed with Backspace. It only reverses observed, counted characters in the current
focused input. Pinyin edits, pasted and historical text are excluded. Focus changes,
mouse clicks, shortcuts, navigation, resets and restarts discard deletion tracking.
Totals, frequencies and speeds are corrected against the original commit date; typing
time is retained. This requires accessible text changes within the existing bounded
context, so some deletions may be missed. No input order or deletion history is stored.

The third Statistics subpage shows today/7-day/30-day/lifetime Han totals and the top
32 characters. Windows subscribes to UI Automation `CompositionFinalized` events.
For providers that omit finalized events, it also checks the focused control's
`TextEditPattern` composition state every 100 ms, reads only the observed composition
range after completion, and pairs duplicate observations across sources. Empty or
whitespace-only ranges do not indicate an active composition.
An independent plain `TextPattern` path supports controls such as QQ whose composition
state can remain active after committing. Real keyboard activity arms this path;
Space, Enter, numbers or punctuation confirm candidates, and actual inserted Han text
is counted. Key counts never estimate character totals. Initial focus only establishes
a baseline; shortcuts, clicks, deletion, navigation and cancellation rebase it.
Only the focused text control's preceding 256 text units (at most 4096 UTF-16 units)
are retained transiently in memory, never in logs or storage, and reads are clamped
to that control's range. Providers such as QQ may return empty selections with distinct
endpoints or carets outside their own text range; the fallback then uses the bounded
tail of the focused input, so edits in the middle of long inputs can be missed.
Mouse candidate selection, input without keyboard confirmation,
rapid focus changes and apps without accessible text can be missed. Subscription alone
does not prove coverage.
Only focused, non-password controls are accepted. No document or clipboard reads occur.
Han characters in other languages may count; punctuation, Latin letters and normal pastes do not.

Recent speed is the Han count committed in the last 60 seconds (characters/minute).
Session average divides Han count by active minutes: each burst starts with one second,
commit gaps up to five seconds count, and longer gaps begin a new burst. These estimates
exclude time before the first commit and are independent of the selected history period.
Restart/reset clears session speeds. Daily histograms retain 30 days and lifetime counts
remain; reset clears them transactionally. Only unordered frequencies persist locally;
event strings are discarded in the callback, and compatibility context stays in memory only.
Existing CSV exports retain their input/app scope.

字数页的「打字计时器」默认关闭，仅记录本次运行。开启开始或继续累计，关闭暂停；
显示打字时间和空闲时间（时:分:秒），与历史时段筛选无关。每次采集到汉字提交后
5 秒内为打字时间，其他开启时间为空闲，未能采集的输入也会被视为空闲。
不追溯开启前的输入；重启清零。重置统计清零时间并保留当前开关。

The Character statistics page has a session-only Typing timer, off by default.
Enabling starts/resumes accumulation; disabling pauses it. Typing and idle durations
use HH:MM:SS, independent of the historical period. Five seconds after each captured
Han commit count as typing; remaining enabled time is idle, including unobserved input.
Earlier input is not billed. Restart clears the timer; Reset statistics clears its times
and preserves the current switch. Timer operations use a separate optional v1 ABI.

## 显示单位 / Display units

展开统计页的「显示单位」，可分别设置键盘／鼠标计数、指针移动和滚轮的单位名称
（最多 16 个 Unicode 字符，留空跟随当前语言），并为移动量和滚轮量设置换算倍率。
显示值 = 原始值 × 倍率，倍率范围为 0.000000001 至 1000000000；点击「应用单位」保存，
也可恢复默认。倍率由用户校准，不会自动将设备输入换算为实际物理距离。
设置随应用配置保留，重置统计时不清除；仅影响统计页，数据库、CSV 和悬浮窗仍使用原始单位。

Expand **Display units** on the Statistics page to customize the count, pointer travel and
scroll labels (up to 16 Unicode characters; empty follows the current language), and separate
travel/scroll multipliers. Display = raw value × multiplier; valid multipliers range from
0.000000001 to 1000000000. Apply to save or restore defaults. Calibration is manual;
this does not infer physical distance. Settings survive statistics resets and affect only
this page; the database, CSV exports and floating panel retain raw units.

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
input values, unordered per-key counts, app IDs/names, icons and durations stay on this device. Han statistics retain only unordered character frequencies, never sentences,
key order, coordinates, window titles, URLs or screenshots.

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

## 键盘热力图 / Keyboard heatmap

各键计数由 Rust 输入核心以固定大小的原子数组累计，通过可选的
`include/bongo_cat/statistics_keyboard_plugin.h` 扩展交给统计工作线程。
SQLite 按日保存无序直方图，并保存累计值；热力图支持今日、7 天、30 天和累计。
旧历史没有各键次数，无法回填。部分物理键（如主键区和数字键区的 Enter）
由平台报告为相同键名，会共享计数；不影响既有快捷键和动画映射。
重置统计会一并清除热力图数据。应用图标在后台缓存于数据目录的 `application-icons`，
不保存可执行文件完整路径；无法读取图标时显示占位图标。

The Rust input core accumulates a fixed-size atomic histogram through the optional
keyboard extension. SQLite stores daily and lifetime counts without event order.
Existing history cannot be backfilled. Keys sharing a platform name, such as Enter
and keypad Enter, share counts; shortcut and animation mappings are unchanged.
Reset also clears heatmap history. Application icons are cached locally under
`application-icons` without storing executable paths. Unavailable icons use a placeholder.
