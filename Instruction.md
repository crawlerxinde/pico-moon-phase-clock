这张 Moon Phase Clock 的新效果图，字体风格主要是 现代无衬线字体（Modern Sans-serif），强调几何感、简洁性、数字可读性，以及不同信息之间的视觉层次。

不过需要先说明：效果图由 AI 生成，并没有实际指定或加载某个字体文件，因此无法准确识别其字体名称。 我可以根据字形特征，给你一套非常接近效果图、可以实际部署在 RP2350 上的开源字体方案。

## 1. 效果图中的字体分析

# 04:05

TIME / 主要时间

主时钟数字

接近 Inter Light / Regular，笔画均匀、数字轮廓简洁，使用较轻字重形成大面积留白。

# 18 Sun

DATE / 日期

日期字体

日期数字采用较高字重，其余文字较轻，接近 Inter SemiBold + Regular 的组合。

## Oct 2026

Mo Tu We Th Fr

CALENDAR / 日历

日历字体

接近 Inter Medium / Regular，字形紧凑清晰，数字排列需要固定单元格宽度。

## 13:28 ↑

ASTRO / 天文时间

日出、月升时间

适合 IBM Plex Mono Regular，数字等宽，方便上下两行对齐。

## 2. 推荐使用的开源字体

Inter

适用于主时钟、日期、城市和日历。现代、简洁，支持等宽数字特性（Tabular Numbers）。

建议字重：Light 300 / Regular 400 / SemiBold 600

[官方字体下载](https://github.com/rsms/inter/releases?utm_source=chatgpt.com) · SIL OFL 1.1&#x20;

[image](https://www.google.com/s2/favicons?domain=https://github.com\&sz=32)

GitHub



# 04:05

18 Sun · Oct 26

字体风格示意

IBM Plex Mono

适合日出、日落、月升、月落等小型数字信息。字符等宽，仪器感更强。

建议字重：Regular 400 / Medium 500

[官方字体下载](https://github.com/IBM/plex/releases?utm_source=chatgpt.com) · SIL OFL&#x20;

[image](https://www.google.com/s2/favicons?domain=https://github.com\&sz=32)

GitHub



## 13:28 ↑

## 22:46 ↓

等宽数字风格示意

JetBrains Mono

IBM Plex Mono 的替代品，小尺寸字符辨识度优秀，但在这张效果图里不是首选。

[官方字体下载](https://github.com/JetBrains/JetBrainsMono/releases?utm_source=chatgpt.com) · SIL OFL 1.1&#x20;

[image](https://www.google.com/s2/favicons?domain=https://github.com\&sz=32)

GitHub



## 06:27 ↑

## 17:31 ↓

等宽字体替代方案

## 3. 针对 640 × 172 的具体字体配置

有一点很重要：之前生成的效果图是概念设计，不是严格按照 640 × 172 像素制作的。真正移植到开发板时，需要明显缩小字号。

以下给出一套适合实际屏幕的初始参数（px）：

| 界面元素                 | 字体            | 字重           | 字号 |
| -------------------- | ------------- | ------------ | -- |
| 主时钟 `04:05`          | Inter         | Light 300    | 66 |
| `PM`                 | Inter         | Medium 500   | 15 |
| 秒数 `05`              | Inter         | Regular 400  | 18 |
| 日期 `18 Sun - Oct/26` | Inter         | Regular 400  | 18 |
| 城市 `Beijing`         | Inter         | Medium 500   | 15 |
| 月份 `Oct 2026`        | Inter         | SemiBold 600 | 15 |
| 星期 `Mo Tu We...`     | Inter         | Medium 500   | 10 |
| 日历数字                 | Inter         | Regular 400  | 11 |
| 日出月落时间               | IBM Plex Mono | Regular 400  | 12 |

字号是建议的字体像素规格，而不是保证每个字形实际占用相同的高度。需要根据你的渲染引擎再校正 baseline、行高和字符间距。

## 4. RP2350 上的实际部署建议

我建议最后只保留 Inter + IBM Plex Mono 两个字体家族，甚至可以进一步缩减为一个 Inter 家族。

如果用 LVGL 或 FreeType，可以从 TTF 生成目标字号的点阵字库，并只保留界面需要的字符，例如：

```
0123456789
: / - ↑ ↓
ABCDEFGHIJKLMNOPQRSTUVWXYZ
abcdefghijklmnopqrstuvwxyz
```

这样能避免把完整字体库放进固件，减少 Flash 和运行时内存消耗。月亮、太阳、位置标志建议用独立图标资源，而不是依赖字体字形。

最终推荐：Inter Light 用于大时间数字，Inter Regular / SemiBold 用于日期和日历，IBM Plex Mono Regular 用于天文时间。 这套组合最接近效果图的优雅、现代和精密仪器风格，也适合 RP2350 这种嵌入式设备。
