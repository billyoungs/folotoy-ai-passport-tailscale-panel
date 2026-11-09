#!/usr/bin/env python3
"""assets/icons/generate_os_icons.py —— 生成主机名前的系统图标点阵(np_os_icons.c)。

为什么不用现成字体:子集字体来自 Noto Sans CJK,里面既没有 🐧 这类 emoji 字形,
LVGL 的单色字体也画不出彩色图标。这里用 12x12 的 A8 掩码手画几枚极简标志,
运行期由 main/np_icons.c 拼成一个回调式 LVGL 字体(回退到 np_font_14)。

字形本体是逐行点阵:12x12 太小,用椭圆/矩形合成的东西只会糊成一团(第一版就是
这样,预览里认不出企鹅和安卓),所以直接按像素排。'#' 满档、'+' 半档(肚皮、屏幕
内容这类要暗一层的部位)、'.' 透明。排完先看 --preview,别把没核对过的点阵写进固件。

用法(仓库根目录):
    python3 assets/icons/generate_os_icons.py            # 生成
    python3 assets/icons/generate_os_icons.py --preview  # 只在终端里看字形
"""
import sys

PX = 12
ALPHA = {".": 0, "~": 90, "+": 150, "#": 255}

GLYPHS = [
    ("NP_GLYPH_LINUX", 0xE000, "企鹅(linux)", [
        # 侧面企鹅:头在身子左上,喙在第 2~3 行往右凸出(第 4 行往里收,凸出才有
        # 下巴那道缺口,否则只是头上一块瘤);翅膀是穿过浅色肚皮的一条深色斜带。
        # 正面直立那版(第一版)在 12 px 下认不出,眼睛留白也一样:1 像素的洞只会
        # 把头劈成两块。
        "....####....",
        "..######....",
        "..#########.",
        "..########..",
        "..######....",
        ".#########..",
        ".#+++++++#..",
        ".#++##+++#..",
        ".#+++##++#..",
        ".#++++##+#..",
        "..#+++++#...",
        "..##...##...",
    ]),
    ("NP_GLYPH_WINDOWS", 0xE001, "视窗(windows)", [
        "............",
        "#####.#####.",
        "#####.#####.",
        "#####.#####.",
        "#####.#####.",
        "#####.#####.",
        "............",
        "#####.#####.",
        "#####.#####.",
        "#####.#####.",
        "#####.#####.",
        "#####.#####.",
    ]),
    ("NP_GLYPH_APPLE", 0xE002, "苹果(darwin/ios/macOS)", [
        "......##++..",
        "......##....",
        "..########..",
        ".##########.",
        ".########...",
        ".#######....",
        ".#######....",
        ".#######....",
        ".########...",
        "..########..",
        "...######...",
        "............",
    ]),
    ("NP_GLYPH_ANDROID", 0xE003, "安卓(android)", [
        "..#......#..",
        "..#......#..",
        "...######...",
        "..########..",
        "..#.####.#..",
        "..########..",
        "#.########.#",
        "#.########.#",
        "#.########.#",
        "..########..",
        "..###..###..",
        "............",
    ]),
    ("NP_GLYPH_OTHER", 0xE004, "通用机器(认不出系统时)", [
        "............",
        "............",
        ".##########.",
        ".#........#.",
        ".#.++++++.#.",
        ".#........#.",
        ".#.+++++..#.",
        ".#........#.",
        ".##########.",
        ".....##.....",
        "...######...",
        "............",
    ]),
]


def to_grid(rows, name):
    if len(rows) != PX:
        raise SystemExit("%s:需要 %d 行,给了 %d 行" % (name, PX, len(rows)))
    grid = []
    for y, row in enumerate(rows):
        if len(row) != PX:
            raise SystemExit("%s 第 %d 行:需要 %d 列,给了 %d 列(%r)"
                             % (name, y, PX, len(row), row))
        bad = sorted({c for c in row if c not in ALPHA})
        if bad:
            raise SystemExit("%s 第 %d 行有未知字符 %s(允许 %s)"
                             % (name, y, "".join(bad), "".join(sorted(ALPHA))))
        grid.append([ALPHA[c] for c in row])
    return grid


def preview(name, grid):
    print(name)
    ramp = {0: " ", 90: ".", 150: "·", 255: "█"}
    for row in grid:
        print("  |" + "".join(ramp[v] for v in row) + "|")
    print()


def emit():
    out = [(name, code, desc, to_grid(rows, name))
           for name, code, desc, rows in GLYPHS]

    lines = []
    lines.append("/* 由 assets/icons/generate_os_icons.py 生成,请勿手改。 */")
    lines.append("/* 重跑:python3 assets/icons/generate_os_icons.py */")
    lines.append('#include "np_icons.h"')
    lines.append("")
    lines.append("const uint32_t np_icon_codes[NP_ICON_COUNT] = {")
    for name, code, desc, _ in out:
        lines.append("    0x%04X,  /* %s %s */" % (code, name, desc))
    lines.append("};")
    lines.append("")
    lines.append("// A8 掩码:每像素 1 字节 alpha,行连续无填充(stride = %d)。" % PX)
    lines.append("const uint8_t np_icon_bits[NP_ICON_COUNT][NP_ICON_PX * NP_ICON_PX] = {")
    for name, _, desc, grid in out:
        lines.append("    {  /* %s %s */" % (name, desc))
        for y, row in enumerate(grid):
            body = ", ".join(str(v) for v in row)
            sep = "," if y < PX - 1 else ""
            lines.append("      %s%s" % (body, sep))
        lines.append("    },")
    lines.append("};")
    lines.append("")
    dest = "assets/icons/np_os_icons.c"
    with open(dest, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines))
    print("已生成 %s(%d 枚字形)" % (dest, len(out)))


if "--preview" in sys.argv:
    for name, code, desc, rows in GLYPHS:
        preview("%s U+%04X %s" % (name, code, desc), to_grid(rows, name))
else:
    emit()
