# 社区模型商店 / Community model store

页面正文、交互及 UI 状态由 Rust 插件的 `src/page_ui.rs` 创建；宿主经
`include/bongo_cat/plugin_ui_page.h` 提供通用控件、绘图和业务服务。
插件内嵌自身 `locales/*.json` 的十种语言，仅解析当前语言；翻译内存由插件分配并释放。
语言加载接口为 `bongo_cat_store_locale_query_v1`（`load/text/visit/release`），
宿主 `bongo_cat_i18n_get` 转发查询，不缓存插件字符串。旧插件缺少页面扩展时需更新。

The Rust plugin owns page layout, interactions, UI state and its ten embedded locale files.
The host supplies generic drawing and application services. The versioned locale interface
loads the active language, lends strings for drawing/font baking, and releases allocations
before unload; the host never caches plugin translation pointers. Older plugins need the
page extension to display settings.

商店由独立 Rust 动态库 `bongo_store` 提供，通过版本化 C ABI 加载。
CMake 的 `BONGO_CAT_BUILD_STORE` 默认开启。插件随其他插件打包到
`plugins/`（macOS 为 `Contents/PlugIns`），可在设置的插件页导入 ZIP／动态库、
启用、停用或移除用户安装的插件。停用后隐藏商店页面，保留已安装模型。

在商店页填写 GitHub 仓库 URL（`https://github.com/owner/repository`），
点击「应用仓库」即可保存并刷新目录。留空后应用可恢复默认仓库。
目录读取目标仓库 `main` 分支根目录的 `model.json`，预览、下载和仓库链接同步切换。
默认目录地址为：
`https://raw.githubusercontent.com/TianMengLucky/Awesome-BongoCat/main/model.json`。
首次打开商店时获取目录，可手动刷新；404 显示「目录尚未发布」。
网络请求复用现有可取消 HTTPS 客户端及 GitHub 下载加速设置。

## model.json

支持对象 `{"models": [...]}` 或直接数组。每项格式：

```json
{
  "models": [
    {
      "name": "示例模型",
      "author": "作者名字",
      "author_url": "https://github.com/example",
      "repository_path": "models/example/model.zip",
      "folder_path": "models/example",
      "preview_path": "models/example/preview.png",
      "sha256": "填写模型 ZIP 文件的 64 位 SHA-256 十六进制值"
    }
  ]
}
```

`author_url` 可省略或为空，此时作者名字不响应点击；提供时必须是 HTTPS URL。
其他字段必填。路径相对于仓库根目录，不能包含绝对路径、反斜杠、`..`、
URL 查询／片段或百分号转义。`repository_path` 是 ZIP 文件，`folder_path`
是包含 ZIP 和预览图的目录，`preview_path` 是具体图片路径。
`sha256` 必须是该 ZIP 文件的 64 位十六进制 SHA-256，支持大小写。

可选字段 `paid`（布尔值，默认 `false`）与 `purchase_url`：`paid` 为 `true` 表示该模型
另有付费完整版，必须在 `purchase_url` 填入其 HTTPS 购买链接，卡片会在作者名下方显示
「购买完整版」并打开该链接；免费模型可省略两者（此时不显示购买链接）。
`purchase_url` 非空时必须是 HTTPS URL。

目录最多 512 个模型、2 MiB；预览图最多 4 MiB；ZIP 最多 512 MiB。
Windows 可用 `Get-FileHash model.zip -Algorithm SHA256` 生成哈希值。
示例中的哈希占位文字必须替换，否则整个目录会被拒绝。

## 下载与卸载

卡片显示预览图、模型名、作者名和绿色下载／卸载按钮。
点击有链接的作者名字会打开作者页面；付费模型在作者名下方显示「购买完整版」链接，
点击打开购买页面。下载及安装在独立后台线程执行。
ZIP 以流式方式写入临时文件，下载完成后重新读取文件校验 SHA-256，
通过后才复用现有模型导入器安装。哈希不匹配／请求取消会删除临时文件；
不自动选择或启动下载的模型。图片解码复用 Rust 安全解码器和现有缩略图缓存。

本地以 ZIP 哈希记录导入返回的模型 ID，重启后据此判断安装状态。
卸载只处理该 ZIP 对应的已安装模型，并沿用模型管理的停用及删除逻辑。
多模型 ZIP 的所有已导入模型都包含在卸载范围内；手动从模型页删除后
商店会恢复下载按钮。目录更新 ZIP 时必须同步更新 SHA-256。

## Implementation notes

The optional Rust `cdylib` parses the catalog through the shared parser under
`bongo-safe/src/store_catalog.rs`; no network JSON is parsed in C.
`include/bongo_cat/store_plugin.h` defines the versioned serialized C ABI
(currently v2: entries carry the paid flag and purchase URL, so the host and the
store plugin must be rebuilt together; v1 plugins are rejected).
The host owns one cancellable worker and uses the existing HTTP, thumbnail,
model import and removal infrastructure. A SHA-256 mismatch prevents import.

No SDK files are included in this plugin. Enabling the store does not fetch the
catalog until the Store page is opened. Disabling it leaves installed models intact.
