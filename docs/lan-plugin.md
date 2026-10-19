# 好友联机插件 / Friends plugin

联机由独立 Rust 动态库 `bongo_lan` 提供，默认不构建；宿主缺少插件时不会联网。
明确启用 `-DBONGO_CAT_BUILD_LAN=ON` 后，构建系统会将插件放入可执行文件旁的
`plugins`（macOS 为 `Contents/PlugIns`）。也可以通过设置的插件导入入口安装动态库。
安装后在侧边栏独立的「好友联机」页面设置；「插件」页面仅保留跳转入口。不会自行打开防火墙端口。

## 使用

1. 每位成员选择各自要展示的模型，安装并启用相应模型渲染插件。
   Live2D 每位成员仍需自行提供 Cubism Core；Core、渲染插件或其他可执行文件不会传输。
2. 创建方选择「创建房间」页签，选择监听 IP 和端口（默认 39027）。
   默认优先使用系统首选的公网临时 IPv6；没有时仅使用本机回环地址，
   需要自行填写局域网 IPv4。不会默认监听所有网络接口。
3. 输入相同的 8–64 字符房间码。最多 7 位好友选择「加入房间」页签，填入创建方的
   IPv4 `192.168.1.10:39027` 或 IPv6 `[地址]:39027` 后连接。
   链路本地 IPv6 可显式输入数字接口范围，例如 `[fe80::1234%12]:39027`；
   `%12` 必须是加入方本机对应接口的编号，不能直接照搬对方编号。
4. 连接后在成员列表分别修改每位好友在本机显示的昵称（最多 32 个字符）；小字显示在朋友猫上方。
   昵称不发送给对方，目前只在本次运行期间保留。可拖动朋友的猫改变其本机位置。
5. 成员退出后移除对应窗口并清理对应缓存，其他成员继续联机。房主退出则关闭整个房间。更换本机模型时会断开，需重新连接。

公网临时 IPv6 列表过滤回环、ULA、链路本地和文档地址，只展示首选隐私地址；
Windows 使用系统随机后缀来源及有效期、Linux 使用地址临时标志、macOS 使用
`IN6_IFF_TEMPORARY`。可手动刷新；不存在时明确提示，不替换成稳定公网地址。
临时地址失效或网络切换后需刷新并重新连接。公网可达性还取决于路由与防火墙。

## 数据和边界

- 最多 8 人（含房主），由房主转发成员模型和姿态；没有外部发现服务器或中继服务。IPv4 仅允许私网、链路本地和回环；IPv6 支持公网单播。
- 模型、资源清单和姿态帧统一经 `snow` 的
  `Noise_NNpsk0_25519_ChaChaPoly_SHA256` 加密认证：房间码派生共享密钥，
  临时 X25519 密钥协商会话，ChaCha20-Poly1305 加密并拒绝篡改与重放。
  每条客户端与房主链路必须完成加密确认后才能读取或传输模型，不支持回退旧明文协议。
  房主解密后重新加密转发，能够读取房间数据；这不是跨房主的端到端加密，应信任房主。
  模型归档另做 SHA-256 校验。请使用长随机房间码；短密码仍可能被离线猜测。
  临时 IPv6 只减少长期地址关联，不能代替身份验证、加密和防火墙。
- 约 30 Hz 发送固定的 16 项手部、鼠标、头部和摇杆姿态，不发送键名、输入文字、
  应用信息或本地路径。远端姿态直接送往独立模型实例，不注入系统输入、快捷键和统计。
- 使用本地模型内容标识和设置文件名查找已安装模型；匹配则复用本地资源。
  缺失时在后台压缩传输模型和适配资源，只解压到 `<cache>/lan/peer-*`。
  不调用模型导入、模型目录扫描或会话模型列表接口。
- 限制为 4096 文件、128 MiB 压缩包、256 MiB 解压数据和 8 MiB 单个 JSON。
  拒绝路径穿越、链接、重复路径、原生库和可执行文件；JSON 资源引用必须为包内相对路径。
  带外部／父目录资源引用的模型需要先整理成自包含模型包。
- 断开／停用时先销毁远端渲染器，再清理缓存、等待线程结束并卸载动态库。
  异常进程终止可能留下系统临时缓存；它们始终不进入用户模型列表。
- 本机所有猫统一使用本机当前后端：OpenGL 共用上下文；Vulkan 共用设备和队列，
  每只猫拥有独立离屏图像和帧命令池。朋友猫不能自行回退至另一后端，
  切换后端时先销毁旧目标，再按新的本机后端重建。双方电脑不要求使用相同后端。
  Vulkan 朋友窗口使用小尺寸 GPU 回读后合成昵称；Windows 复用分层透明呈现，
  Linux 通过 SDL 软件表面呈现最终图像。模型绘制本身始终使用 Vulkan。
  模型渲染插件不可用时保留连接并每五秒重试显示。
  支持标准姿态参数的模型可同步动作，模型自定义动作／表情和物品未跨端同步。

## 验证

遵循仓库约定，本次不构建、不启动应用，也不执行会触发编译的测试。
已加入三成员模型与姿态转发、单人退出与缓存延迟清理、ABI、姿态边界、加密握手、拒绝明文、密文篡改／重放、Vulkan 设备所有权、非法归档、取消及公网临时地址筛选测试。
获得构建授权后运行 `lan-rust`、`lan-safe-rust`、`vulkan-offscreen-ownership`，并用至少三台设备验证 IPv4/IPv6、
缺失模型、断线清理、渲染插件不可用及昵称显示。

## English

Build explicitly with `-DBONGO_CAT_BUILD_LAN=ON`, or install the `bongo_lan` shared
library through Plugins. Configure the connection on the dedicated Friends / LAN sidebar page; Plugins only links to it.
One user hosts and up to seven friends join using the same room code (8–64 characters) and
`IPv4:port` or `[IPv6]:port`. Default port: 39027. The default listener uses a
preferred public temporary IPv6 address when available, otherwise loopback;
enter a LAN IPv4 manually when needed. No firewall rules are modified.

Create and Join use separate tabs with independent address/code drafts. Addresses are editable
and can be copied with one click. Leaving a member keeps everyone else connected;
leaving the host closes the room.

The UI lists preferred public temporary IPv6 addresses only; it never substitutes
a stable public address. Refresh after network changes or address expiry. IPv6
link-local scopes must use the joining device's own numeric interface index.
Privacy addresses reduce persistent tracking; they do not secure a connection.
Noise (`snow`, `Noise_NNpsk0_25519_ChaChaPoly_SHA256`) authenticates the room
code and encrypts manifests, models and poses using X25519 and ChaCha20-Poly1305.
The host decrypts and re-encrypts relayed data and must be trusted; encryption
is per client-to-host link, not end-to-end across the host.
An encrypted confirmation is mandatory before model IO. There is no plaintext
fallback; old protocol peers must update. SHA-256 additionally checks archives.
Use a long random room code: weak passwords remain susceptible to offline guessing.

Use the member list to edit each local friend nickname (up to 32 characters) to show small text above the
remote pet. Names last for the current application run. Drag the remote pet to
move it locally. Changing the local model disconnects the session; reconnect to
share the new model. Fixed model pose values are sampled at about 30 Hz; no key
names, typed text, application data, paths or OS input injection are involved.
Custom motions, expressions and items are not synchronized.

Matching installed models are reused. Missing models and adapter assets are
compressed in a worker and extracted only under `<cache>/lan/peer-*`, never
installed in the user's catalog. The host retires the remote renderer before
releasing session cache ownership. Limits are 4096 files, 128 MiB compressed,
256 MiB expanded, and 8 MiB per JSON. Traversal, links, duplicate paths, native
libraries and executables are rejected. Resource references must stay inside the
package. A process crash may leave temporary cache files, but never catalog entries.

All local pets follow the local backend: OpenGL shares the context, while
Vulkan shares the device/queue and owns separate target images and frame pools.
The remote pet never falls back to a different renderer; backend switches retire
borrowed resources first. The two computers may choose different backends.
Vulkan model drawing remains on the GPU. A small readback composites the nickname;
Windows reuses layered presentation and Linux presents the final SDL software surface.
Both users need the
appropriate enabled renderer and, for Live2D, their own Cubism Core. Renderer
libraries and Core are never transferred. Missing renderers retry every five
seconds without losing the connection. Builds and compiled tests were not run;
run the registered `lan-rust`, `lan-safe-rust` and `vulkan-offscreen-ownership` tests and a three-device integration
check once a build is authorized.
