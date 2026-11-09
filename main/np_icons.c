// main/np_icons.c —— 把 12x12 A8 点阵挂成 LVGL 9 的回调式静态位图字体。
// 走 static_bitmap + A8 这条快路:lv_draw_sw_letter 直接把掩码当 alpha 用,
// 不分配 draw_buf、不解码,所以列表二十几行同时带图标也不会碰内部堆。
#include "np_icons.h"

static int find_glyph(uint32_t letter)
{
    for (int i = 0; i < NP_ICON_COUNT; i++) {
        if (np_icon_codes[i] == letter) return i;
    }
    return -1;
}

static bool icon_get_glyph_dsc(const lv_font_t *font, lv_font_glyph_dsc_t *dsc_out,
                               uint32_t letter, uint32_t letter_next)
{
    (void)font;
    (void)letter_next;
    const int idx = find_glyph(letter);
    if (idx < 0) return false;   // 交给 fallback 链继续找

    // gid.index 是取位图时唯一的线索:draw 阶段只把 dsc 传回回调。
    dsc_out->gid.index = (uint32_t)idx;
    dsc_out->format = LV_FONT_GLYPH_FORMAT_A8;
    dsc_out->box_w = NP_ICON_PX;
    dsc_out->box_h = NP_ICON_PX;
    dsc_out->stride = NP_ICON_PX;      // A8:每像素 1 字节,行无填充
    dsc_out->adv_w = NP_ICON_PX + 2;   // 图标后留 2 px,别贴住主机名
    dsc_out->ofs_x = 0;
    // 0 = 方框底边正好落在基线上(np_font_14 行高 16、基线以下 3)。
    dsc_out->ofs_y = 0;
    dsc_out->is_placeholder = 0;
    return true;
}

static const void * icon_get_glyph_bitmap(lv_font_glyph_dsc_t *g_dsc,
                                          lv_draw_buf_t *draw_buf)
{
    (void)draw_buf;
    const uint32_t idx = g_dsc->gid.index;
    if (idx >= NP_ICON_COUNT) return NULL;
    return np_icon_bits[idx];
}

const lv_font_t np_icon_font = {
    .dsc = NULL,
    .fallback = NULL,
    .user_data = NULL,
    .get_glyph_dsc = icon_get_glyph_dsc,
    .get_glyph_bitmap = icon_get_glyph_bitmap,
    .release_glyph = NULL,   // 掩码是编译期数据,没有需要释放的缓存项
    .line_height = 16,
    .base_line = 3,
    .cap_height = 10,
    .x_height = 7,
    .subpx = LV_FONT_SUBPX_NONE,
    .kerning = LV_FONT_KERNING_NONE,
    .static_bitmap = 1,
    .underline_position = -2,
    .underline_thickness = 1,
};

static lv_font_t s_font_runtime;

void np_icons_attach(lv_font_t *base, const lv_font_t *tail)
{
    // 链:base → 图标 → tail。改的是副本的指针,np_icon_font 本身保持 const。
    s_font_runtime = np_icon_font;
    s_font_runtime.fallback = tail;
    base->fallback = &s_font_runtime;
}

int np_icon_utf8(uint32_t glyph, char *out)
{
    out[0] = '\0';
    if (find_glyph(glyph) < 0) return 0;
    // 私有区码点都在 0xE000..0xEFFF,固定是 3 字节 UTF-8 形式。
    out[0] = (char)(0xE0 | (glyph >> 12));
    out[1] = (char)(0x80 | ((glyph >> 6) & 0x3F));
    out[2] = (char)(0x80 | (glyph & 0x3F));
    out[3] = '\0';
    return 3;
}
