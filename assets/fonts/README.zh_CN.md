<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# fonts/

可复用字体材料与生成的 LVGL 字体源码。

## 网络面板子集字体(`np_font_14.c`、`np_font_20.c`)

| 项目 | 取值 |
| --- | --- |
| 来源 | 系统包 `google-noto-sans-cjk-vf-fonts` 中 `NotoSansCJK-VF.ttc` 的 `Noto Sans CJK SC`、字重 400 静态实例 |
| 许可证 | SIL Open Font License 1.1 —— 允许再分发子集 |
| 覆盖范围 | 可打印 ASCII(`0x20-0x7E`)加 `netpanel_chars.txt` 列出的全部码点 |
| 字符清单 | `main/np_inventory.c` 与 `netpanel_chars.txt`,由 `extract_chars.py` 从应用源码生成 |
| 转换工具 | `lv_font_conv@1.5.3`,`--bpp 4 --format lvgl --no-compress`,14 与 20 两档 |
| 去向 | 编入 `main` 组件(见 `main/CMakeLists.txt`),`netpanel_ui.c` 使用并以 Montserrat 14 作回退 |

`.ttc` 是 CFF2 可变字体集合,lv_font_conv 读不了,脚本按固定顺序走,不能换:

1. 先用 `fontTools.varLib.instancer` 钉住 `wght=400`,得到静态 CFF2 `.otf`
   (按 `$STATIC_OTF` 缓存;整族 6.5 万字形要十几分钟)。这一步必须在子集化
   之前 —— 对裁过的 CFF2 字体跑 instancer 会抛 `TypeError`。
2. 用 `pyftsubset` 把静态实例裁到清表码点。
3. 用 `fontTools.cffLib.CFF2ToCFF` 把 CFF2 轮廓改写成 CFF。转换器要求 TopDict
   已展开,所以转换前必须先摸一遍 `FDSelect`、`CharStrings` 和每个 `Private`,
   并关掉 `recalcBBoxes`。
4. `lv_font_conv` 逐字号输出 `.c`。

仓库根目录端到端重新生成:

```bash
python3 assets/fonts/extract_chars.py <应用源码列表...>   # 刷新清表
FONT_ENV=/含-fonttools 的-venv 路径 \
  ./assets/fonts/generate_netpanel_fonts.sh <应用源码列表...>
```

`extract_chars.py --check`(已接入 `tools/validate.sh`)在跟踪源码里出现清表
缺失字符时会使门禁失败;设备端 UI 启动时还会做一次字形自检并打印缺字数量。
清表增长后必须重跑本脚本,并确认新增码点出现在生成的字形表里再提交。

## 约定

- 生成的 `.c` 字体随仓库提交,构建机无需上游 `.ttc` 也能复现。
- 不要把完整的 Noto `.otf`/`.ttc` 二进制放进这里,上游是系统包。
- 界面文案变更时,`netpanel_chars.txt`、`main/np_inventory.c` 与字体 `.c` 需同一次提交。
- 列表行首的操作系统图标**不**走这条流水线:它们在 [`../icons/README.zh_CN.md`](../icons/README.zh_CN.md) 里,
  用私有区码点,且刻意不进 `netpanel_chars.txt` —— 那是图形不是文字。
