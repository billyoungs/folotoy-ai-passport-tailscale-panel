#!/usr/bin/env python3
"""官方 SVG 与 main/np_logo.c 几何表的一致性检查(主机侧,不需要 ESP-IDF)。

mark 是从 assets/icons/tailscale-favicon.svg 描来的,那张表一旦偏离官方文件,
屏幕上画的就是"看着像但不是"的图形,而固件照样编得过、C 单测也只认自己那份表。
这里以官方文件为准绳对账:坐标、半径、前景/背景分组,以及由它们推出的盒边长。
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SVG = ROOT / "assets/icons/tailscale-favicon.svg"
TABLE = ROOT / "main/np_logo.c"
HEADER = ROOT / "main/np_logo.h"

# 官方文件里两个分组的可见度:<g id="t"> 是前景,<g id="grid"> 是 opacity 0.4 的背景。
GROUPS = (("t", False), ("grid", True))


def fail(msg):
    print("FAIL: " + msg)
    sys.exit(1)


def svg_dots(text):
    """按分组顺序取出 (cx, cy, r);r 必须等于 ry,否则就不是正圆。"""
    dots = []
    for gid, faded in GROUPS:
        m = re.search(r'<g\s+id\s*=\s*"%s"\s*>(.*?)</g>' % gid, text, re.S)
        if not m:
            fail("官方 SVG 里找不到 <g id=\"%s\"> 分组" % gid)
        found = re.findall(
            r'<ellipse\s+cx="([\d.]+)"\s+cy="([\d.]+)"\s+rx="([\d.]+)"\s+ry="([\d.]+)"\s*/?>',
            m.group(1))
        if not found:
            fail("<g id=\"%s\"> 里没有 <ellipse> 点" % gid)
        for cx, cy, rx, ry in found:
            if float(rx) != float(ry):
                fail("圆心 (%s, %s) 的 rx/ry 不等(%s/%s),不是正圆" % (cx, cy, rx, ry))
            dots.append((float(cx), float(cy), float(rx), faded))
    return dots


def table_dots(text):
    m = re.search(r"np_logo_dots\[NP_LOGO_DOTS\]\s*=\s*\{(.*?)\n\};", text, re.S)
    if not m:
        fail("np_logo.c 里找不到 np_logo_dots 表")
    rows = re.findall(r"\{\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(true|false)\s*\}", m.group(1))
    if not rows:
        fail("np_logo.c 的表里没有 { cx, cy, faded } 行")
    return [(int(cx), int(cy), r == "true") for cx, cy, r in rows]


def define(text, name):
    m = re.search(r"#define\s+%s\s+(\d+)" % name, text)
    if not m:
        fail("np_logo.h 里找不到 #define %s" % name)
    return int(m.group(1))


def main():
    svg = SVG.read_text(encoding="utf-8")
    vb = re.search(r'viewBox="0\s+0\s+([\d.]+)\s+([\d.]+)"', svg)
    if not vb or float(vb.group(1)) != float(vb.group(2)):
        fail("官方 SVG 的 viewBox 不是正方形,换算方式要重写")

    dots = svg_dots(svg)
    radius = dots[0][2]
    if any(d[2] != radius for d in dots):
        fail("官方 SVG 里各点半径不一致")

    # 点只占画布中间一块;表用的是"内容区左上角为原点"的坐标,内容区由点自身推出来。
    origin = min(min(d[0], d[1]) - radius for d in dots)
    extent = max(max(d[0], d[1]) + radius for d in dots) - origin
    if origin != int(origin) or extent != int(extent) or radius != int(radius):
        fail("官方 SVG 的内容区/半径不是整数像素,1 单位 = 1 像素的换算不成立")

    want = [(int(d[0] - origin), int(d[1] - origin), d[3]) for d in dots]
    got = table_dots(TABLE.read_text(encoding="utf-8"))
    if got != want:
        fail("np_logo.c 的表是 %s,官方文件换算过来是 %s" % (got, want))

    head = HEADER.read_text(encoding="utf-8")
    if define(head, "NP_LOGO_DOTS") != len(want):
        fail("NP_LOGO_DOTS 与点数(%d)不符" % len(want))
    if define(head, "NP_LOGO_BOX") != int(extent):
        fail("NP_LOGO_BOX 与内容区边长(%d)不符" % int(extent))
    if define(head, "NP_LOGO_R") != int(radius):
        fail("NP_LOGO_R 与官方半径(%d)不符" % int(radius))

    print("tailscale logo contract: ok (%d dots, box %d, r %d)"
          % (len(want), int(extent), int(radius)))


if __name__ == "__main__":
    main()
