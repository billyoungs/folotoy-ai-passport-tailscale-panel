// main/np_logo.c —— np_logo.h 的几何表,值直接来自官方 SVG 的圆心减去内容区原点
// (例:实心点 cx=15 → 15-12=3)。改这张表要先改 assets/icons/tailscale-favicon.svg,
// tests/test_logo_contract.py 会拿两边的坐标逐点对账。
#include "np_logo.h"

const np_logo_dot_t np_logo_dots[NP_LOGO_DOTS] = {
    // <g id="t">:中间一行三点 + 正中下方一点,官方文件里是全不透明的前景。
    { 3, 12, false },
    { 12, 12, false },
    { 21, 12, false },
    { 12, 21, false },
    // <g id="grid">:顶行三点 + 底部两角,opacity 0.4 的背景网格。
    { 3, 3, true },
    { 12, 3, true },
    { 21, 3, true },
    { 3, 21, true },
    { 21, 21, true },
};
