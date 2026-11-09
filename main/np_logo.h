// main/np_logo.h —— Tailscale 官方 mark 的几何表:9 枚圆点,照 assets/icons/
// tailscale-favicon.svg 描。纯数据,不碰 LVGL;绘制在 netpanel_ui.c。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define NP_LOGO_DOTS 9
// 官方文件是 48 单位 viewBox,圆点只占 x/y ∈ 12..36 的 24 单位内容区;
// 这里按 1 SVG 单位 = 1 像素描,所以绘制盒就是 24×24。
#define NP_LOGO_BOX 24
#define NP_LOGO_R   3   // 官方 rx=ry=3

typedef struct {
    int16_t cx, cy;   // 圆心,相对绘制盒原点
    bool faded;       // 官方文件里 opacity:0.4 的那一组
} np_logo_dot_t;

extern const np_logo_dot_t np_logo_dots[NP_LOGO_DOTS];
