# 音效插件 / Keyboard sound effects

职责边界及新宿主要求的 companion ABI 见 [附加插件职责边界](plugin-boundaries.md)。
宿主与随附插件需一起更新；业务实现不在宿主回退。
See [add-on plugin boundaries](plugin-boundaries.md) for ownership and required companion
ABIs. Update the host and bundled plugins together; no host business fallback is used.


页面正文、交互及 UI 状态由 Rust 插件的 `src/page_ui.rs` 创建；宿主经
`include/bongo_cat/plugin_ui_page.h` 提供通用控件、绘图和业务服务。
插件内嵌自身 `locales/*.json` 的十种语言，仅解析当前语言；翻译内存由插件分配并释放。
语言加载接口为 `bongo_cat_sounds_locale_query_v1`（`load/text/visit/release`），
宿主 `bongo_cat_i18n_get` 转发查询，不缓存插件字符串。旧插件缺少页面扩展时需更新。

The Rust plugin owns page layout, interactions, UI state and its ten embedded locale files.
The host supplies generic drawing and application services. The versioned locale interface
loads the active language, lends strings for drawing/font baking, and releases allocations
before unload; the host never caches plugin translation pointers. Older plugins need the
page extension to display settings.

在「设置 → 插件」启用「音效」，即可打开独立的「音效」页面。

- 页面显示主键区、功能键、导航区、方向键和数字键盘。
- 点击任意按键：已有绑定先试听，然后弹出系统文件选择窗口。
- 选择 WAV、MP3、Ogg Vorbis 或 FLAC 后，在后台验证、复制和保存，随后试听。
- 鼠标悬浮显示绑定的音频文件名；已绑定按键带颜色与标记。
- 选中按键后也可点击「选择音频」「试听」「清除绑定」。取消弹窗保留原绑定。
- 实际键盘按下时播放该键的音效；松开后可再次触发，系统自动重复事件不重复播放。
- 同时敲击不同按键可叠加播放，沿用现有播放器的有界声部池；不依赖所选模型。

每个插件有自己的常驻工作线程和任务队列：`bongo-sounds-worker` 负责音效，
`bongo-items-worker` 负责物品，统计仍使用独立 Rust 工作线程。
它们不共用任务执行线程。音效线程负责绑定读取、音频复制／SHA-256／Rust 解码、
播放设备及声部、绑定保存和退出清理；界面只提交任务并读取受短锁保护的内存状态。
磁盘、解码和设备操作不在状态锁内执行。物品导入和保存同样在自己的线程执行。

音效弹窗打开、快捷键录制／模态操作及 smoke 输入屏蔽期间不播放物理键音效；
松键状态仍被消费。队列最多 256 项，满时略过新事件，排空后复位按键状态，避免丢失松键导致卡住。
任务进行期间不能停用或卸载；应用退出先排空并等待各自线程退出，再销毁实例及卸载库。
输入权限沿用宿主；Wayland 等环境可能无法提供全局键盘输入。
键位使用宿主规范名称，与键盘布局的文字或 IME 输出无关。宿主把左右 Win 映射为同一 `Meta`，
主 Enter 与数字键盘 Enter 映射为同一 `Return`，这些位置共享绑定。
当前仅主桌宠启用音效插件，避免多进程重复播放和覆盖配置。

音频限制为 64 MB，并沿用 bongo-safe 的解码样本上限（48 kHz 双声道约五分钟）。
音频存放于 `<data>/sounds/<sha256>.snd`，绑定为 `<data>/sounds/bindings.json`。
替换／清除绑定先持久保存，失败会恢复旧绑定；临时文件原子替换原配置。
停用或卸载保留数据。损坏配置不会被覆盖；修复后重新启用插件。
音效使用独立播放器，停用时停止自身声音，不停止模型或其他插件的音频。

默认构建 `BONGO_CAT_BUILD_SOUNDS=ON`；设为 `OFF` 可不附带库，宿主仍支持导入。
实现为 `src/rust/bongo-sounds/` 下的 Rust cdylib crate（`[lib] name = "bongo_sounds"`），
导出 `bongo_cat_sounds_query`；键位表与校验逻辑与 ABI 一并从原 C 实现逐字节移植。
Windows `bongo_sounds.dll`、macOS `libbongo_sounds.dylib`、Linux `libbongo_sounds.so`。
插件页面可导入动态库或仅包含一个本平台插件的 ZIP。
ABI 版本 1，见 [`sounds_plugin.h`](../include/bongo_cat/sounds_plugin.h)。
JSON 和音频解析复用 bongo-safe；播放复用 miniaudio，不引入额外解析器。

CTest `sounds-rust` 覆盖键位唯一性、无效文件名拒绝、重复事件、独立按键、清除和恢复原子性；
`plugin-workers` 验证阻塞一个插件的线程不影响另一个线程，并验证线程退出。
本次未自行构建或执行这些测试。

## English

Enable Sound effects on Plugins. The page shows a keyboard including function keys,
navigation, arrows and numpad. Click a key to preview its existing clip and open a native
file picker. Choosing WAV/MP3/Ogg Vorbis/FLAC queues import, validation, durable binding
save and preview. Hover shows the filename. Preview, replace and clear are also available
for the selected key. Canceling leaves the binding intact.

Physical key down edges play their bindings; autorepeat is suppressed. Different keys
can overlap through the existing bounded voice pool, independently of the model.
The existing input permissions/layout limitations apply. Left/right Win share Meta;
main and keypad Enter share Return because these are the host's canonical input names.
Only the primary pet activates the plugin.

Sounds and Items each have their own persistent worker and bounded queue, separate from
the Rust statistics worker. Sound file I/O, SHA-256, Rust decoding, audio device/voices,
binding load/save and cleanup run on the sound worker. UI state locks cover memory copies
and mutations, never decoding, disk or device operations. Items import/save run on the
item worker. Shutdown drains and joins each worker before instance destruction/unload.
Active tasks temporarily prevent disable/removal. The queue holds 256 jobs; overflow drops
new input and clears held state after draining. Picker/modal input consumes transitions
without physical sound playback.

Imported files live in `<data>/sounds/<sha256>.snd`; bindings live in `bindings.json`.
Imports are limited to 64 MB and the existing bongo-safe decoded sample cap. Atomic durable
replacement saves bindings; failures restore previous in-memory bindings. Disable/uninstall
preserves data and stops only this plugin's independent audio player. Corrupt configuration
is preserved and cannot be overwritten through the page until repaired and re-enabled.

`BONGO_CAT_BUILD_SOUNDS=ON` bundles the v1 dynamic plugin by default. The implementation is a
Rust cdylib crate under `src/rust/bongo-sounds/` (`[lib] name = "bongo_sounds"`) exporting
`bongo_cat_sounds_query`; the key table and validation were ported byte for byte from the
former C source. Windows `bongo_sounds.dll`, macOS `libbongo_sounds.dylib` and Linux
`libbongo_sounds.so` keep their names. Set OFF to omit the bundled library.
Import a platform library or single-plugin ZIP through Plugins. CTest sounds-rust covers
keyboard mappings and binding/transition safety; plugin-workers covers independent progress
and orderly shutdown. Neither test was built or run in this change.
