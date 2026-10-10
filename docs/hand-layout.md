# 手部、气泡和字符裁剪配置 / Hand, frame and character crops

Mver 模型在 OpenGL、Vulkan 和 Metal 后端可将按键图片拆成手部、气泡外框和字符。外框和尾巴随模型镜像，字符保持可读；关闭「多按键显示」后也保留这一行为。开启多按键显示时，每侧的字符按按下顺序用 `+` 连接，放进同一个随内容加宽的气泡，只绘制第一个显示按键的手部。超出显示数量的按键保留按住状态，松开前面的按键后自动补位。

在手部图片所在的文件夹中新建 `hand-layout.json`：标准模式为 `img/standard/hand/hand-layout.json`；键盘或手柄模式分别使用对应模式下的 `lefthand/hand-layout.json` 和 `righthand/hand-layout.json`。配置在导入或重新扫描模型时复制到运行时适配器的 `resources/left-keys` 或 `resources/right-keys`，重新启动应用即可重新扫描。请编辑原模型文件夹中的配置。

使用 [示例配置](hand-layout.example.json) 作为起点。示例覆盖「A-赛琳娜」标准模式全部 90 张 1000×1000 图片的手部、气泡和字符。字段含义如下：

| 字段 | 含义 |
| --- | --- |
| `schemaVersion` | 配置版本，当前为 `1` |
| `hand.x`, `hand.y` | 手部裁剪框左上角，原图左上角为 `(0, 0)` |
| `hand.width`, `hand.height` | 手部裁剪框宽、高，单位为原图像素 |
| `bubble.x`, `bubble.y` | 包含外框、尾巴和字符的完整气泡裁剪框左上角 |
| `bubble.width`, `bubble.height` | 完整气泡裁剪框宽、高 |
| `characters.x`, `characters.y` | 气泡内部字符区域的左上角，仍以原图左上角为原点 |
| `characters.width`, `characters.height` | 字符区域宽、高，必须完全位于 `bubble` 内 |
| `gap` | 加号左右各一侧的像素间距；`0` 隐藏加号并取消间距 |
| `maxKeys` | 每侧最多显示的按键数量，范围 `1` 至 `8`，默认 `5` |

字符裁剪框应覆盖文件夹内所有字符，并在字符周围留几像素空白。程序从气泡的主背景色中分离字符，保留连接裁剪框边缘的外框线条，将原位置的字符擦除后重绘到共享气泡内。适用于背景色一致、字符与外框分离的气泡。气泡水平分三段加宽，保留两端外框和尾巴的形状。

一侧的配置应用于该文件夹中所有按键图片，因此手部和气泡裁剪框也应覆盖所有姿势的边缘。右侧没有独立配置时使用左侧布局的水平对称裁剪位置；右侧有配置时，其坐标直接按右侧原图左上角计算。内容宽度超过画布时，气泡、字符和间距一起缩小。

配置使用标准 JSON，不能包含注释或重复字段；坐标须为非负整数，宽高须为正整数。缺省字段保留默认值；旧配置仅设置 `bubble` 时，会按其比例推算 `characters`，建议补全明确的字符裁剪框。错误配置会记录警告并保留默认布局。图片容纳不下裁剪框、无法分离字符或纹理上传失败时，保留原图单键显示，此时字符随原图翻转。

## English

Mver on OpenGL, Vulkan and Metal splits key sprites into a hand, a bubble frame and characters. The frame and tail mirror with the model, while characters stay readable, also when Multi-Key Display is disabled. With the option enabled, each side joins held-key characters in press order with `+` inside one bubble that widens to fit. The first displayed key supplies the hand pose. Held keys beyond the display limit reappear as earlier keys are released.

Place `hand-layout.json` beside the hand PNGs: `img/standard/hand/` for standard mode, or the current keyboard/gamepad mode’s `lefthand/` and `righthand/` folders. Import or model discovery copies it into the runtime adapter’s `resources/left-keys/` or `resources/right-keys/`. Restart after editing the source model to rescan it. Use the [example](hand-layout.example.json), measured across all 90 Selena standard-mode sprites, as a starting point.

`hand`, `bubble` and `characters` each contain `x`, `y`, `width` and `height`, measured in original image pixels from the top-left. `bubble` covers the entire frame, tail and characters. `characters` must fit inside it and leave a few blank pixels around every glyph in the folder. Glyph extraction uses the dominant background color and preserves outline strokes connected to the crop’s edges. It supports bubbles with a uniform fill and characters separated from the outline. Three horizontal frame slices retain the end shapes while stretching the center.

`gap` is the spacing on each side of `+`; zero hides the separator and removes its spacing. `maxKeys` limits displayed keys per side to 1–8 (default 5). Keep all regions large enough for every sprite. A missing right-side file mirrors the left layout; an explicit right-side file uses coordinates from that image’s top-left. Frames, glyphs and spacing shrink together to fit the logical canvas width.

Use standard JSON without comments or duplicate keys. Coordinates must be nonnegative integers and dimensions must be positive integers. Omitted fields retain defaults. Older files that specify only `bubble` derive the character region proportionally; an explicit `characters` crop is recommended. Malformed layouts log a warning and retain the default layout. Sprites that cannot fit the crops, yield no separated glyphs, or fail to upload keep the original single-key overlay, including its mirrored characters.
