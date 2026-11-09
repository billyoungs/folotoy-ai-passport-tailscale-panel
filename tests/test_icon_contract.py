#!/usr/bin/env python3
"""图标点阵与字体约定的一致性检查(主机侧,不需要 ESP-IDF)。

np_icons.c 用一条固定公式把码点写成 3 字节 UTF-8,而它接受的码点来自生成的
np_os_icons.c;np_model.h 里的 NP_GLYPH_* 又是第三处。三处只要有一处单方面改动,
屏幕上出现的就是 "?" 或空白,固件照样能编出来。这里把这些跨文件约定钉死。
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DATA = ROOT / "assets/icons/np_os_icons.c"
MODEL = ROOT / "main/np_model.h"

ALLOWED_ALPHA = (0, 90, 150, 255)


def fail(msg):
    print("FAIL: " + msg)
    sys.exit(1)


def parse_codes(text):
    m = re.search(r"np_icon_codes\[NP_ICON_COUNT\]\s*=\s*\{(.*?)\};", text, re.S)
    if not m:
        fail("np_os_icons.c 里找不到 np_icon_codes 表")
    return [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{4}),", m.group(1))]


def parse_bits(text):
    m = re.search(r"np_icon_bits\s*\[[^\]]*\]\[NP_ICON_PX \* NP_ICON_PX\]\s*=\s*\{(.*)\n\};",
                  text, re.S)
    if not m:
        fail("np_os_icons.c 里找不到 np_icon_bits 表")
    # 每枚字形一个 "{ /* 注释 */ ... }" 块,块与块之间用 "}," 分隔。
    return [[int(v) for v in re.findall(r"\b(\d{1,3})\b", chunk)]
            for chunk in m.group(1).split("\n    },") if chunk.strip()]


def main():
    text = DATA.read_text(encoding="utf-8")
    codes = parse_codes(text)
    glyphs = parse_bits(text)

    if len(codes) != 5 or len(glyphs) != 5:
        fail("期望 5 枚字形,得到 %d 个码点 / %d 个点阵" % (len(codes), len(glyphs)))

    # np_icons.c 的编码器只覆盖 3 字节 UTF-8 的私有区。
    for cp in codes:
        if not 0xE000 <= cp <= 0xEFFF:
            fail("码点 U+%04X 超出 np_icon_utf8 的 3 字节假设(要求 E000..EFFF)" % cp)

    for i, px in enumerate(glyphs):
        if len(px) != 144:
            fail("第 %d 枚字形有 %d 个像素,应为 12x12=144" % (i, len(px)))
        bad = sorted({v for v in px if v not in ALLOWED_ALPHA})
        if bad:
            fail("第 %d 枚字形有非约定 alpha 值 %s" % (i, bad))
        if all(v == 0 for v in px):
            fail("第 %d 枚字形是空的" % i)

    want = [int(v, 16) for v in re.findall(
        r"#define\s+NP_GLYPH_\w+\s+0x([0-9A-Fa-f]{4})u",
        MODEL.read_text(encoding="utf-8"))]
    if want != codes:
        fail("np_model.h 的 NP_GLYPH_* 是 %s,点阵表是 %s"
             % (["%04X" % c for c in want], ["%04X" % c for c in codes]))

    print("icon glyph contract: ok (%d glyphs, %s)"
          % (len(codes), " ".join("U+%04X" % c for c in codes)))


if __name__ == "__main__":
    main()
