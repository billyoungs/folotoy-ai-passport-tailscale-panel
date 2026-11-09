// main/np_icons.h —— 系统图标字体:私有区码点 → 12x12 A8 点阵的回调式 LVGL 字体。
// 点阵数据由 assets/icons/generate_os_icons.py 生成(assets/icons/np_os_icons.c),
// 本文件只负责把它挂成字体。码点与系统的对应关系见 np_model.h 的 np_os_glyph。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#define NP_ICON_PX    12
#define NP_ICON_COUNT 5

extern const uint32_t np_icon_codes[NP_ICON_COUNT];
extern const uint8_t np_icon_bits[NP_ICON_COUNT][NP_ICON_PX * NP_ICON_PX];

// 只含 5 枚图标的字体:落到 np_font_14 之后的 fallback 链上才能用(见 np_icons_install)。
extern const lv_font_t np_icon_font;

// 把图标字体接到 base 之后:base → np_icon_font → tail。
// 字体是回调式的静态结构,这里只改副本的 fallback 指针,不动生成的数据。
void np_icons_attach(lv_font_t *base, const lv_font_t *tail);

// 码点的 UTF-8 前缀写进 out(至少 4 字节,含 NUL),返回写入的字节数。
// glyph 为 0 或不在表里时 out 为空串、返回 0,调用方据此不加前缀。
int np_icon_utf8(uint32_t glyph, char *out);
