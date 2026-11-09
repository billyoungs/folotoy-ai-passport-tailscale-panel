<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# icons/

网络面板设备名称前面那几枚操作系统标志,以及把它们变成 C 数据的脚本。

| 文件 | 角色 |
| --- | --- |
| [`generate_os_icons.py`](generate_os_icons.py) | 唯一事实来源:每枚字形是一份 12 字符的 ASCII 网格,外加 `--preview` 预览与写出步骤。 |
| [`np_os_icons.c`](np_os_icons.c) | 生成的两张表(`np_icon_codes`、`np_icon_bits`),经 `main/CMakeLists.txt` 编进固件。不要手改。 |
| [`tailscale-favicon.svg`](tailscale-favicon.svg) | Tailscale 官方 mark 的原文副本,取自 `https://tailscale.com/favicon.svg`。只作参考:几何照描进 `main/np_logo.c`,本文件不编进固件。 |

## 格式与接入

- 一枚字形是 12 × 12 网格,每像素 1 字节,alpha 只允许 0 / 90 / 150 / 255
  (`.` `~` `+` `#`)。这个尺寸下半个像素的边缘要么认得出要么认不出,没有抗锯
  齿这一步可发明。
- `main/np_icons.c` 用一个回调式 `lv_font_t` 把这些表交给渲染:
  `LV_FONT_GLYPH_FORMAT_A8` 配 `static_bitmap = 1`,这正是 LVGL 9.6 软件渲染器
  直接混合原始掩码、不复制进绘制缓冲的那条快路。列表一次只渲染 6 行,这 6 行
  除了本来就要的标签之外不额外花堆。
- 码点用私有区 `U+E000`~`U+E004`;`np_model.h` 给它们起名
  (`NP_GLYPH_LINUX` 等),`np_os_glyph()` 把控制面返回的 `os` 字符串映射到其
  中一枚。字体链是 `np_font_14 → np_icon_font → montserrat_14`,接在子集字体描
  述符的运行期副本上,不改 const 全局。
- `tests/test_icon_contract.py` 钉住字形数量、私有区范围(三字节 UTF-8 编码建
  在这个前提上)、每枚的像素数、只允许那四档 alpha,以及 `NP_GLYPH_*` 常量与生
  成表的顺序一致。

这些码点刻意不进 [`../fonts/extract_chars.py`](../fonts/extract_chars.py) 维护
的字形清表:它们是图形不是文字,而生成脚本自己的注释里有绝不能变成字形的字符。
给调用方留下的后果是:图标字节必须拼在 `np_text_sanitize()` *之后*,否则会被过
滤成 `?`。

## 重新生成

在仓库根目录:

```sh
python3 assets/icons/generate_os_icons.py --preview   # 先看图腾
python3 assets/icons/generate_os_icons.py             # 重写 np_os_icons.c
```

没有外部依赖,也不做字体转换。改了网格就把生成的文件一起提交;
`tools/validate.sh` 不会替你重跑这一步。

## 来源与许可

**操作系统标志** —— 原创绘制:网格是为本仓库手摆出来的 12 px 图形,并特意保持
成上面那种 ASCII 形式,不用图像工具也能审。这几枚网格里没有从任何厂商资源文件
里提取、描摹或随附的内容,也不重新分发任何第三方图标集。这些标志指的是 Tailscale 控制面
`os` 字段所表达的操作系统;涉及的商标归各自所有者,本仓库不构成授权。

**Tailscale 官方标志** —— `tailscale-favicon.svg` 是相反的情形,这里也照实标注:它是相反的情形,这里也照实标注:它是
官方文件的逐字节副本,只在开头加了一段记录来源与抓取日期的注释,`main/np_logo.c`
照它描出坐标。Tailscale 与其图形是 Tailscale Inc. 的商标,留在这里只为说明本机
监控的是哪个服务;本仓库与 Tailscale Inc. 无隶属关系,也不代表其认可。若官方品
牌规范日后要求移除,删掉这份 SVG 以及 `main/np_logo.c`、`main/np_logo.h`、
`tests/test_np_logo.c`、`tests/test_logo_contract.py` 和 `main/netpanel_ui.c` 里的
`status_logo()` 那一段即可,状态页其余部分不依赖它们。
