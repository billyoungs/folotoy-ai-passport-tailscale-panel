// main/netpanel_ui.c —— 深色工具风界面,不复用 demo 的 ui_pixel 壳。
// 字体:assets/fonts 生成的 Noto 子集(ASCII+清表汉字),缺失时回退 Montserrat。
#include "netpanel_ui.h"

#include "lvgl.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "app_cfg.h"
#include "bsp_display.h"
#include "np_display_cfg.h"
#include "np_icons.h"
#include "np_inventory.h"
#include "np_logo.h"
#include "np_menu.h"
#include "np_text.h"
#include "np_time.h"

#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(np_font_14);
LV_FONT_DECLARE(np_font_20);
LV_FONT_DECLARE(lv_font_montserrat_28);
LV_FONT_DECLARE(lv_font_montserrat_48);

#define UI_BG        0x0B1220u
#define UI_CARD      0x101A2Du
#define UI_CARD_SEL  0x1D3557u
#define UI_TEXT      0xE2E8F0u
#define UI_MUTED     0x94A3B8u
#define UI_ONLINE    0x22C55Eu
#define UI_OFFLINE   0x64748Bu
#define UI_ERROR     0xF87171u
#define UI_ACCENT    0x3B82F6u
#define UI_DIVIDER   0x1E293Bu
#define UI_ROW_LINE  0x18243Au
#define UI_BAR_OFF   0x2B3B55u

static lv_font_t s_font14, s_font20;
static bool s_font_ready;

static lv_obj_t *s_list_scr, *s_head, *s_clock, *s_area, *s_footer;
static lv_obj_t *s_rows[NP_MAX_DEVICES];
static lv_obj_t *s_dots[NP_MAX_DEVICES];
static lv_obj_t *s_names[NP_MAX_DEVICES];
static lv_obj_t *s_wifi[3], *s_wifi_dot;
static lv_obj_t *s_batt_body, *s_batt_fill, *s_batt_tip;
static lv_obj_t *s_detail_scr, *s_status_scr;
static lv_obj_t *s_detail_body;   // 详情主体的滚动容器(值换行后靠它滚)
static lv_obj_t *s_actions[NP_MENU_COUNT];
static lv_obj_t *s_status_hint;      // 状态页底部那一行:临时提示直接改写它
static char s_hint_text[64];
static volatile np_view_t s_view;
static size_t s_selected;
static size_t s_action_sel;
// 系统设置屏:主菜单三行(亮度/熄屏间隔/熄屏显示),OK 进入子模式,长按返回列表。
static lv_obj_t *s_settings_scr;
static size_t s_setting_sel;
static lv_obj_t *s_brightness_val;   // 亮度子模式下的大数值标签(只更新不重建)
static size_t s_interval_sel;
static size_t s_net_sel;             // 网络配置子菜单选中下标(复用 np_menu 契约)
static lv_obj_t *s_aod_scr, *s_aod_time, *s_aod_online;

// ------------------------------------------------------------- 字体与自检
static void fonts_init(void)
{
    if (s_font_ready) return;
    s_font14 = np_font_14;
    s_font20 = np_font_20;
    // 图标字体插在 Montserrat 之前:标签照旧用子集汉字,私有区码点落到点阵图标。
    np_icons_attach(&s_font14, &lv_font_montserrat_14);
    np_icons_attach(&s_font20, &lv_font_montserrat_20);
    s_font_ready = true;

    // 清表自检:逐个码点确认子集真的带字形,缺字会显示成占位框。
    size_t missing = 0;
    for (size_t i = 0; i < np_inventory_count; i++) {
        lv_font_glyph_dsc_t g;
        if (!lv_font_get_glyph_dsc(&s_font14, &g, np_inventory[i], 0) ||
            g.is_placeholder) {
            missing++;
        }
    }
    if (missing) {
        ESP_LOGE("np_ui", "字体缺字 %u 个,重跑 extract_chars + 字体生成",
                 (unsigned)missing);
    }
}

static lv_obj_t *mk_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    return scr;
}

// 简化标签创建。
static lv_obj_t *mk_label(lv_obj_t *parent, int x, int y, uint32_t color,
                          const lv_font_t *font)
{
    lv_obj_t *lb = lv_label_create(parent);
    lv_obj_set_style_text_color(lb, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(lb, font, 0);
    lv_obj_set_pos(lb, x, y);
    return lb;
}

static lv_obj_t *mk_divider(lv_obj_t *parent, int y)
{
    lv_obj_t *ln = lv_obj_create(parent);
    lv_obj_set_size(ln, 216, 1);
    lv_obj_align(ln, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_color(ln, lv_color_hex(UI_DIVIDER), 0);
    lv_obj_set_style_border_width(ln, 0, 0);
    lv_obj_set_style_radius(ln, 0, 0);
    lv_obj_remove_flag(ln, LV_OBJ_FLAG_SCROLLABLE);
    return ln;
}

// ---------------------------------------------------------------- 状态栏图标
// Wi-Fi:三段同心 + 中心一个点。子集字体里没有 Wi-Fi 字形,显示状态用画出来的。
// 电池:外壳 + 内填充 + 右侧凸点。百分比数字不再上屏(用户要求图形化并靠右)。
// 两枚图标共用一条视觉带:上下都夹在 14..26 之间,中线 20,正好和状态栏文字
// 的墨迹中线一致。Wi-Fi 只画同心弧的上半扇形,墨迹是 CY-10.5(外弧顶)到
// CY+2(中心点下缘),带宽 12.5,中线在 CY-4.25 —— 带中心 20 反推出 WIFI_CY=24。
// 电池是 12 px 高的实心矩形,直接让外壳顶边贴带上缘。挪图标要整条带一起挪,
// 别只挪一枚:np-0.1.21 之前电池按文字基线摆在 19,看上去比 Wi-Fi 低半截。
#define ICON_TOP 14
#define WIFI_CX  186
#define WIFI_CY  (ICON_TOP + 10)
#define BATT_X   206   // 外壳左上角;最右到 233,留 6 px 屏幕边距
#define BATT_Y   ICON_TOP
#define BATT_W   24
#define BATT_H   12
#define BATT_PAD 2     // 外壳描边厚度,填充在里面

static void wifi_create(lv_obj_t *scr)
{
    // 图标固定在 (WIFI_CX, WIFI_CY):中心是 x1 + size/2,所以位置要往前让 size/2。
    static const int32_t sz[3] = { 21, 15, 9 };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *arc = lv_arc_create(scr);
        lv_obj_set_size(arc, sz[i], sz[i]);
        lv_obj_set_pos(arc, WIFI_CX - sz[i] / 2, WIFI_CY - sz[i] / 2);
        lv_arc_set_bg_angles(arc, 225, 315);   // 225→315 经过 270(正上方)
        lv_arc_set_angles(arc, 0, 0);          // 指示段不画
        lv_obj_set_style_arc_width(arc, 2, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
        lv_obj_set_style_pad_all(arc, 0, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_set_style_border_width(arc, 0, LV_PART_KNOB);
        lv_obj_set_style_shadow_width(arc, 0, LV_PART_KNOB);
        lv_obj_set_clickable(arc, false);
        s_wifi[i] = arc;
    }
    s_wifi_dot = lv_obj_create(scr);
    lv_obj_set_size(s_wifi_dot, 4, 4);
    lv_obj_set_pos(s_wifi_dot, WIFI_CX - 2, WIFI_CY - 2);
    lv_obj_set_style_radius(s_wifi_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_wifi_dot, 0, 0);
    lv_obj_remove_flag(s_wifi_dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_clickable(s_wifi_dot, false);
}

// 强度只决定点亮几段,dBm 数字不上界面。
static void wifi_update(int8_t rssi)
{
    if (!s_wifi[0]) return;
    const int lit = np_signal_bars(rssi);
    for (int i = 0; i < 3; i++) {
        lv_obj_set_style_arc_color(s_wifi[i],
            lv_color_hex(i < lit ? UI_TEXT : UI_BAR_OFF), LV_PART_MAIN);
    }
    lv_obj_set_style_bg_color(s_wifi_dot,
                              lv_color_hex(lit ? UI_TEXT : UI_BAR_OFF), 0);

    // 没连上网时一段都不亮,只画图标看不出状态,直接写上"未连网"。
    for (int i = 0; i < 3; i++) {
        if (lit) lv_obj_remove_flag(s_wifi[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_wifi[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (lit) lv_obj_remove_flag(s_wifi_dot, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_wifi_dot, LV_OBJ_FLAG_HIDDEN);
}

static void battery_create(lv_obj_t *scr)
{
    // 三个独立对象(不用父子):子对象会跟着父对象的圆角被裁,填充反而露边。
    s_batt_body = lv_obj_create(scr);
    lv_obj_set_size(s_batt_body, BATT_W, BATT_H);
    lv_obj_set_pos(s_batt_body, BATT_X, BATT_Y);
    lv_obj_set_style_bg_opa(s_batt_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_batt_body, BATT_PAD, 0);
    lv_obj_set_style_border_color(s_batt_body, lv_color_hex(UI_MUTED), 0);
    lv_obj_set_style_radius(s_batt_body, 3, 0);
    lv_obj_remove_flag(s_batt_body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_clickable(s_batt_body, false);

    s_batt_tip = lv_obj_create(scr);
    lv_obj_set_size(s_batt_tip, 3, 5);
    lv_obj_set_pos(s_batt_tip, BATT_X + BATT_W + 1, BATT_Y + (BATT_H - 5) / 2);
    lv_obj_set_style_bg_color(s_batt_tip, lv_color_hex(UI_MUTED), 0);
    lv_obj_set_style_radius(s_batt_tip, 1, 0);
    lv_obj_set_style_border_width(s_batt_tip, 0, 0);
    lv_obj_remove_flag(s_batt_tip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_clickable(s_batt_tip, false);

    s_batt_fill = lv_obj_create(scr);
    lv_obj_set_size(s_batt_fill, 1, BATT_H - 2 * BATT_PAD);
    lv_obj_set_pos(s_batt_fill, BATT_X + BATT_PAD, BATT_Y + BATT_PAD);
    lv_obj_set_style_radius(s_batt_fill, 1, 0);
    lv_obj_set_style_border_width(s_batt_fill, 0, 0);
    lv_obj_remove_flag(s_batt_fill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_clickable(s_batt_fill, false);
}

// 电量档位:低电红、中间蓝、充足绿;读不到(-1)就只留一个暗外壳,不编数字。
static void battery_update(int pct)
{
    if (!s_batt_body) return;
    const uint32_t shell = pct >= 0 ? UI_MUTED : UI_BAR_OFF;
    lv_obj_set_style_border_color(s_batt_body, lv_color_hex(shell), 0);
    lv_obj_set_style_bg_color(s_batt_tip, lv_color_hex(shell), 0);
    if (pct < 0) {
        lv_obj_set_width(s_batt_fill, 1);
        return;
    }
    uint32_t color = UI_ONLINE;
    if (pct <= 20) color = UI_ERROR;
    else if (pct <= 45) color = UI_ACCENT;
    const int inner_w = BATT_W - 2 * BATT_PAD;
    int fill_w = (inner_w - 1) * pct / 100;
    if (fill_w < 1) fill_w = 1;   // 再低的电也留一格,别让填充看着像没画
    lv_obj_set_width(s_batt_fill, fill_w);
    lv_obj_set_style_bg_color(s_batt_fill, lv_color_hex(color), 0);
}

// ------------------------------------------------------------- 列表视图
// 行内横向几何:卡片 216 宽、2 px 边框,子对象偏移从内容区(边框之内)起算,
// 所以视觉上还要再加 2 px。
#define ROW_DOT_X   10   // 状态点左缘 → 卡片内 12 px
#define ROW_NAME_X  25   // 名称左缘   → 卡片内 27 px
#define ROW_NAME_W  176  // 到卡片内缘 214 还留 11 px,长名字收省略号
#define ROW_NAME_H  17   // np_font_14 的行高:钉死一行,卡片不会被顶开

void np_ui_list_enter(void)
{
    fonts_init();
    s_view = NP_VIEW_LIST;

    s_list_scr = mk_screen();

    // 状态栏左格:日期(M/D)。没校时、没连网时同一格改说当前卡在哪一步,
    // 优先讲"未连网"——断网时 SNTP 也不会有结果,那句才是用户要先处理的。
    // 中格是居中的 HH:MM:标签框按屏幕中线 120 摆,字在框内居中,所以时间是
    // 真的对齐屏幕中线,而不是"看着差不多在中间"。
    s_head = mk_label(s_list_scr, 12, 12, UI_MUTED, &s_font14);
    lv_label_set_text(s_head, "待校时");
    lv_obj_set_width(s_head, 56);
    lv_label_set_long_mode(s_head, LV_LABEL_LONG_CLIP);

    s_clock = mk_label(s_list_scr, 70, 12, UI_TEXT, &s_font14);
    lv_obj_set_width(s_clock, 100);
    lv_obj_set_style_text_align(s_clock, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_clock, LV_LABEL_LONG_CLIP);
    lv_label_set_text(s_clock, "--:--");
    wifi_create(s_list_scr);
    battery_create(s_list_scr);
    mk_divider(s_list_scr, 40);

    s_area = lv_obj_create(s_list_scr);
    lv_obj_set_pos(s_area, 12, 48);
    lv_obj_set_size(s_area, 216, 222);
    lv_obj_set_style_bg_opa(s_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_area, 0, 0);
    lv_obj_set_style_radius(s_area, 8, 0);
    // 主题给每个普通容器套了卡片内边距,行会被整段推右、右边超出屏幕(np-0.1.18
    // 的"主机名边框超出屏幕右边")。几何要按这里的数字算,内边距清零。
    lv_obj_set_style_pad_all(s_area, 0, 0);
    lv_obj_set_flex_flow(s_area, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_area, 5, 0);
    lv_obj_set_scrollbar_mode(s_area, LV_SCROLLBAR_MODE_OFF);

    for (size_t i = 0; i < NP_MAX_DEVICES; i++) {
        lv_obj_t *row = lv_obj_create(s_area);
        lv_obj_set_size(row, 216, 32);
        lv_obj_set_style_bg_color(row, lv_color_hex(UI_CARD), 0);
        lv_obj_set_style_radius(row, 10, 0);
        lv_obj_set_style_border_width(row, 2, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(UI_ROW_LINE), 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        s_rows[i] = row;
        s_dots[i] = lv_obj_create(row);
        lv_obj_set_size(s_dots[i], 10, 10);
        lv_obj_set_style_radius(s_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s_dots[i], 2, 0);
        lv_obj_set_style_border_color(s_dots[i], lv_color_hex(UI_OFFLINE), 0);
        lv_obj_align(s_dots[i], LV_ALIGN_LEFT_MID, ROW_DOT_X, 0);
        // 名称和状态点都按 LEFT_MID 定位,共用同一条中线。偏移都从卡片内容区
        // (边框 2 px 之内)算起,宽度收到卡片右缘内侧,长名字用省略号收尾。
        s_names[i] = mk_label(row, ROW_NAME_X, 0, UI_TEXT, &s_font14);
        lv_obj_align(s_names[i], LV_ALIGN_LEFT_MID, ROW_NAME_X, 0);
        lv_obj_set_width(s_names[i], ROW_NAME_W);
        lv_obj_set_height(s_names[i], ROW_NAME_H);   // 钉一行:长值不许把卡片顶开
        lv_label_set_long_mode(s_names[i], LV_LABEL_LONG_DOT);
        lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
    }

    mk_divider(s_list_scr, 278);
    s_footer = mk_label(s_list_scr, 12, 284, UI_MUTED, &s_font14);
    lv_obj_set_width(s_footer, 216);
    lv_label_set_long_mode(s_footer, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_footer, "上下选择 确定详情 长按菜单");

    lv_screen_load(s_list_scr);
}

void np_ui_list_exit(void)
{
    if (s_detail_scr) { lv_obj_delete(s_detail_scr); s_detail_scr = NULL; }
    if (s_status_scr) { lv_obj_delete(s_status_scr); s_status_scr = NULL; }
    // 状态页整屏删掉后,这些指针指向的就是已释放的对象:必须一并交回 NULL,
    // 否则下一次"只改提示行"会写进一块已经还给堆内存的地方。
    s_status_hint = NULL;
    s_hint_text[0] = '\0';
    for (size_t i = 0; i < NP_MENU_COUNT; i++) s_actions[i] = NULL;
    if (s_list_scr) {
        lv_obj_delete(s_list_scr);
        s_list_scr = NULL;
    }
    s_head = s_clock = s_area = s_footer = NULL;
    s_detail_body = NULL;
    for (int i = 0; i < 3; i++) s_wifi[i] = NULL;
    s_wifi_dot = NULL;
    s_batt_body = s_batt_fill = s_batt_tip = NULL;
    s_view = NP_VIEW_LIST;
    for (size_t i = 0; i < NP_MAX_DEVICES; i++) {
        s_rows[i] = s_dots[i] = s_names[i] = NULL;
    }
}

void np_ui_highlight(void)
{
    for (size_t i = 0; i < NP_MAX_DEVICES; i++) {
        if (!s_rows[i]) continue;
        const bool sel = i == s_selected;
        lv_obj_set_style_bg_color(s_rows[i],
            lv_color_hex(sel ? UI_CARD_SEL : UI_CARD), 0);
        // 选中卡片描一层主色,比只换底色更容易认出来。边框宽度不变:改宽度
        // 会移动内容,选择时行会跳。
        lv_obj_set_style_border_color(s_rows[i],
            lv_color_hex(sel ? UI_ACCENT : UI_ROW_LINE), 0);
        // 一屏只有六行,选到第 7 台以后不把它滚进视野就看不见光标在哪。
        // 越界选中的行还是隐藏态,坐标是旧的,不能拿它算滚动目标。
        if (sel && !lv_obj_has_flag(s_rows[i], LV_OBJ_FLAG_HIDDEN))
            lv_obj_scroll_to_view(s_rows[i], LV_ANIM_OFF);
    }
}

void np_ui_render_list(const np_list_t *data, bool time_synced, int64_t now,
                       int32_t tz_offset_s, int battery_pct, int8_t rssi,
                       const char *err_text, const char *note_text,
                       int64_t sync_epoch, bool fresh)
{
    if (!s_list_scr || data == NULL) return;
    fonts_init();

    char buf[16];
    if (!rssi) snprintf(buf, sizeof(buf), "未连网");
    else if (time_synced) np_time_format_md(buf, sizeof(buf), now, tz_offset_s);
    else snprintf(buf, sizeof(buf), "待校时");
    lv_label_set_text(s_head, buf);
    if (time_synced) {
        np_time_format_hm(buf, sizeof(buf), now, tz_offset_s);
    } else {
        snprintf(buf, sizeof(buf), "--:--");   // 钟没校上就不编一个时间出来
    }
    lv_label_set_text(s_clock, buf);
    battery_update(battery_pct);
    wifi_update(rssi);

    const size_t shown = data->count < NP_MAX_DEVICES ? data->count : NP_MAX_DEVICES;
    if (s_selected >= shown && shown > 0) s_selected = shown - 1;
    char label[NP_NAME_LEN + 4];   // 名称 + 3 字节图标前缀
    for (size_t i = 0; i < NP_MAX_DEVICES; i++) {
        if (!s_rows[i]) continue;
        if (i < shown) {
            const np_device_t *d = &data->items[i];
            lv_obj_remove_flag(s_rows[i], LV_OBJ_FLAG_HIDDEN);
            char pre[4];
            np_icon_utf8(np_os_glyph(d->os), pre);
            snprintf(label, sizeof(label), "%s%s", pre, d->name);
            lv_label_set_text(s_names[i], label);
            // 实心 = 还连着,空心圈 = 已断。除了颜色再多一层形状,屏幕发白
            // 或侧着看时也能分出来。
            // 快照不再描述"现在"时也画成断:数据里那个 online 是抓它那一刻的
            // 事实,链路一断它就只是过去 —— 页脚说连不上、点却还绿着,是同一屏
            // 上两句互相打架的话(np-0.1.27 报的就是这个)。
            const bool live = d->online && fresh;
            lv_obj_set_style_bg_color(s_dots[i], lv_color_hex(UI_ONLINE), 0);
            lv_obj_set_style_bg_opa(s_dots[i], live ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_color(s_dots[i],
                lv_color_hex(live ? UI_ONLINE : UI_OFFLINE), 0);
            // 离线条目整条用次要色,还活着的机器一眼能扫出来。
            lv_obj_set_style_text_color(s_names[i],
                lv_color_hex(live ? UI_TEXT : UI_MUTED), 0);
        } else {
            lv_obj_add_flag(s_rows[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    np_ui_highlight();

    char foot[160];
    if (err_text && err_text[0]) {
        snprintf(foot, sizeof(foot), "异常:%s", err_text);
        lv_obj_set_style_text_color(s_footer, lv_color_hex(UI_ERROR), 0);
    } else if (note_text && note_text[0]) {
        // 正在等一件事完成不是故障:等校时、等地址这类走青字,不冠"异常"。
        snprintf(foot, sizeof(foot), "%s", note_text);
        lv_obj_set_style_text_color(s_footer, lv_color_hex(UI_ACCENT), 0);
    } else {
        // 常规档把入口写清楚:菜单不在详情页里等着人找,长按确定就是。
        // 同步时刻一并写上:在线是抓取那一刻的事实,没有时刻就说不清这屏
        // 信息多新。钟没校上时 epoch 为 0,省略这一段。
        char sync_hm[8];
        if (sync_epoch > 0)
            np_time_format_hm(sync_hm, sizeof(sync_hm), sync_epoch, tz_offset_s);
        else
            sync_hm[0] = '\0';
        np_menu_footer(foot, sizeof(foot), (uint32_t)np_list_online_count(data),
                       (uint32_t)shown, data->total, sync_hm);
        lv_obj_set_style_text_color(s_footer, lv_color_hex(UI_MUTED), 0);
    }
    lv_label_set_text(s_footer, foot);
}

// ------------------------------------------------------------- 详情视图
// 详情的固定部分是标题、状态行、分隔线和底部提示;中间十行装进一个可垂直滚动
// 的容器:值换行时行高跟着长,上下键按单行行距滚动、到边界停住,回列表只有长按
// 确定。
#define BODY_Y      60
#define BODY_H      222
#define BODY_W      240
#define KEY_X       12
#define VAL_X       70
#define VAL_W       (BODY_W - VAL_X - 12)  // 值右缘到 228,和分隔线对齐
#define ROW_GAP     5
#define ROW_STEP    22                     // 上下键一次的滚动量(单行行距)

// 落一行"键 | 值",返回下一行的 y。值按实际样式测量高度,而不是假定一行:
// np-0.1.18 用 LONG_DOT + 自动高度,长值折成两行 34 px 压在下一行上,段落就
// 重叠了。这里改成显式换行(LONG_WRAP),下一行的 y 按实测高度推进。
static int detail_row(lv_obj_t *body, int y, const char *key, const char *val)
{
    lv_obj_t *k = mk_label(body, KEY_X, y, UI_MUTED, &s_font14);
    lv_label_set_text(k, key);
    lv_obj_t *v = mk_label(body, VAL_X, y, UI_TEXT, &s_font14);
    lv_obj_set_width(v, VAL_W);
    lv_label_set_long_mode(v, LV_LABEL_LONG_WRAP);
    lv_label_set_text(v, val);

    lv_point_t sz;
    lv_text_get_size(&sz, val, &s_font14,
                     lv_obj_get_style_text_letter_space(v, LV_PART_MAIN),
                     lv_obj_get_style_text_line_space(v, LV_PART_MAIN),
                     VAL_W, LV_TEXT_FLAG_NONE);
    int h = sz.y > ROW_NAME_H ? sz.y : ROW_NAME_H;
    return y + h + ROW_GAP;
}

// 子网路由取前两条 + 条数尾巴:缓冲区只有 44 字节,三条 CIDR 装不下,
// 全量在配置网页和串口日志里都有。
static void join_routes(const char (*cidr)[NP_ROUTE_LEN], uint8_t n,
                        char *out, size_t cap)
{
    out[0] = '\0';
    for (uint8_t i = 0u; i < n && i < 2u; i++) {
        if (i) strlcat(out, " ", cap);
        strlcat(out, cidr[i], cap);
    }
    if (n > 2u) strlcat(out, " …", cap);
}

void np_ui_show_detail(const np_list_t *data, size_t idx, bool time_synced,
                       int64_t now, int32_t tz_offset_s,
                       const np_detail_t *det, const char *det_note,
                       bool fresh)
{
    if (data == NULL || idx >= data->count) return;
    fonts_init();
    const np_device_t *d = &data->items[idx];
    // 快照不再描述"现在"时,"在线"这一行改说"上次在线":这一页没有那颗点可以
    // 画成空心圈,但同一条判定得管用 —— 列表行已经退了色,详情不能还写在线。
    const bool live = d->online && fresh;

    // 扩展信息晚到一拍会重画整屏,滚动位置要留住,不然用户正看着"中继"就被
    // 弹回顶部。
    const int32_t keep_y = s_detail_body ? lv_obj_get_scroll_y(s_detail_body) : 0;
    if (s_detail_scr) lv_obj_delete_async(s_detail_scr);
    s_detail_scr = mk_screen();
    s_detail_body = NULL;

    char pre[4];
    np_icon_utf8(np_os_glyph(d->os), pre);
    char title[NP_NAME_LEN + 4];
    snprintf(title, sizeof(title), "%s%s", pre, d->name);
    lv_obj_t *ttl = mk_label(s_detail_scr, 12, 10, UI_TEXT, &s_font20);
    lv_obj_set_width(ttl, 216);
    lv_label_set_long_mode(ttl, LV_LABEL_LONG_DOT);
    lv_label_set_text(ttl, title);

    lv_obj_t *state = mk_label(s_detail_scr, 12, 38,
                               live ? UI_ONLINE
                                    : (d->online ? UI_MUTED : UI_OFFLINE),
                               &s_font14);
    lv_obj_set_width(state, 216);
    lv_label_set_long_mode(state, LV_LABEL_LONG_DOT);
    char state_buf[80];
    if (d->online) {
        // 快照过期就不写"在线":那是抓取那一刻的事实,不是现在的事实。
        if (fresh) snprintf(state_buf, sizeof(state_buf), "在线");
        else snprintf(state_buf, sizeof(state_buf), "上次在线");
    } else {
        // 状态行自己说清"离线"和多久,别把话头留给下面一行"见上"。
        char ago[24];
        if (time_synced && d->last_seen)
            np_time_format_ago(ago, sizeof(ago), now, d->last_seen);
        else
            ago[0] = '\0';
        if (ago[0]) snprintf(state_buf, sizeof(state_buf), "离线 %s", ago);
        else snprintf(state_buf, sizeof(state_buf), "离线");
    }
    if (!d->authorized) strlcat(state_buf, " 未授权", sizeof(state_buf));
    if (d->external) strlcat(state_buf, " 外部", sizeof(state_buf));
    if (d->update_available) strlcat(state_buf, " 可更新", sizeof(state_buf));
    if (d->ephemeral) strlcat(state_buf, " 临时", sizeof(state_buf));
    if (det) {
        if (det->ssh) strlcat(state_buf, " SSH", sizeof(state_buf));
        if (det->mapping_varies) strlcat(state_buf, " NAT变", sizeof(state_buf));
    }
    lv_label_set_text(state, state_buf);
    mk_divider(s_detail_scr, 58);

    lv_obj_t *body = lv_obj_create(s_detail_scr);
    lv_obj_set_pos(body, 0, BODY_Y);
    lv_obj_set_size(body, BODY_W, BODY_H);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_set_style_pad_all(body, 0, 0);   // 卡片内边距会把整段推歪
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    s_detail_body = body;

    char val[64];
    int y = 0;
    y = detail_row(body, y, "地址", d->addr[0] ? d->addr : "-");

    if (det && det->distro[0]) {
        snprintf(val, sizeof(val), "%s %s", d->os, det->distro);
    } else {
        snprintf(val, sizeof(val), "%s", d->os[0] ? d->os : "-");
    }
    y = detail_row(body, y, "系统", val);

    y = detail_row(body, y, "客户端", d->ver[0] ? d->ver : "-");
    y = detail_row(body, y, "归属", d->owner[0] ? d->owner : "-");
    y = detail_row(body, y, "标签", d->tags[0] ? d->tags : "无");

    if (d->key_expiry_off) {
        snprintf(val, sizeof(val), "不过期");
    } else if (d->expires && time_synced) {
        char md[24];
        np_time_format_mdhm(md, sizeof(md), d->expires, tz_offset_s);
        snprintf(val, sizeof(val), "%s (%d天)", md,
                 np_time_days_until(d->expires, now));
    } else {
        snprintf(val, sizeof(val), "-");
    }
    y = detail_row(body, y, "密钥", val);

    if (!d->online && d->last_seen && time_synced) {
        np_time_format_mdhm(val, sizeof(val), d->last_seen, tz_offset_s);
    } else if (d->ctrl > 0) {
        // 控制面说连着时不给断线时刻,这里补一句它给的那句话。
        snprintf(val, sizeof(val), "控制面连着");
    } else if (d->ctrl == 0) {
        snprintf(val, sizeof(val), "控制面断开");
    } else {
        snprintf(val, sizeof(val), "-");
    }
    y = detail_row(body, y, "上次在线", val);

    if (d->created && time_synced) {
        np_time_format_mdhm(val, sizeof(val), d->created, tz_offset_s);
    } else {
        snprintf(val, sizeof(val), "-");
    }
    y = detail_row(body, y, "加入", val);

    if (det && det->derp[0]) {
        if (det->derp_ms >= 0)
            snprintf(val, sizeof(val), "DERP %s %dms · %u 端点",
                     det->derp, (int)det->derp_ms, (unsigned)det->endpoints);
        else
            snprintf(val, sizeof(val), "DERP %s · %u 端点",
                     det->derp, (unsigned)det->endpoints);
    } else if (det) {
        // 客户端没上报连接信息(离线或旧版本时确实没有这段)。
        snprintf(val, sizeof(val), "未上报");
    } else {
        snprintf(val, sizeof(val), "-");
    }
    y = detail_row(body, y, "中继", val);

    if (det) {
        char cidrs[44];
        if (det->n_en) {
            join_routes(det->routes_en, det->n_en, cidrs, sizeof(cidrs));
            strlcpy(val, cidrs, sizeof(val));
        } else if (det->n_adv) {
            // 只宣告没生效:多半是没被 ACL 批准,标出来才好判断。
            join_routes(det->routes_adv, det->n_adv, cidrs, sizeof(cidrs));
            snprintf(val, sizeof(val), "宣告 %s", cidrs);
        } else {
            snprintf(val, sizeof(val), "无");
        }
    } else {
        snprintf(val, sizeof(val), "-");
    }
    y = detail_row(body, y, "路由", val);

    // 子对象坐标要等布局才算得出来,滚动范围靠它们;先把这一屏算完再恢复
    // 位置,否则范围会被当成 0,恢复被夹回顶部。
    if (keep_y > 0) {
        lv_obj_update_layout(body);
        lv_obj_scroll_to_y(body, keep_y, LV_ANIM_OFF);
    }

    lv_obj_t *hint = mk_label(s_detail_scr, 12, 288, UI_MUTED, &s_font14);
    lv_obj_set_width(hint, 216);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_DOT);
    if (det_note && det_note[0]) {
        lv_obj_set_style_text_color(hint, lv_color_hex(UI_ACCENT), 0);
        lv_label_set_text(hint, det_note);
    } else {
        lv_obj_set_style_text_color(hint, lv_color_hex(UI_MUTED), 0);
        lv_label_set_text(hint, "上下滚动 长按确定回列表");
    }

    lv_screen_load(s_detail_scr);
    s_view = NP_VIEW_DETAIL;
}

// ------------------------------------------------------------- 状态视图
// 选中态和列表行同一套配色:只靠 2 px 边框在深色卡片上几乎看不出来,而边框宽度
// 一变,居中的文字就跟着跳。宽度恒为 2,换的是颜色。
static void paint_actions(void)
{
    for (size_t i = 0; i < NP_MENU_COUNT; i++) {
        if (!s_actions[i]) continue;
        const bool sel = i == s_action_sel;
        lv_obj_set_style_bg_color(s_actions[i],
            lv_color_hex(sel ? UI_CARD_SEL : UI_CARD), 0);
        lv_obj_set_style_border_color(s_actions[i],
            lv_color_hex(sel ? UI_ACCENT : UI_ROW_LINE), 0);
    }
}

// 只改底部提示行,不重建整页:重建会把正文和选中项一起抹掉,而"再按一次确定 =
// 清除凭据"要的恰恰是选中项原地不动(np-0.1.22 实测:第二拍落到 0 号 = 刷新)。
// 传 NULL 或空串退回默认文案。调用方须持有 LVGL 锁。
void np_ui_status_hint(const char *text)
{
    if (!s_status_hint) return;
    if (text && text[0]) strlcpy(s_hint_text, text, sizeof(s_hint_text));
    else s_hint_text[0] = '\0';
    lv_label_set_text(s_status_hint,
                      s_hint_text[0] ? s_hint_text : "上下选择 确定执行 长按返回");
}

// ------------------------------------------------------------- 状态页标题
// Tailscale 官方 mark 就是 9 枚圆点(几何照 assets/icons/tailscale-favicon.svg 描,
// 表在 np_logo.c)。这个尺寸下点阵图标会糊成一团,所以走 s_wifi_dot 那套 lv_obj 圆盘。
#define LOGO_X 12
#define LOGO_Y 11
#define TITLE_X (LOGO_X + NP_LOGO_BOX + 6)

static void status_logo(lv_obj_t *scr)
{
    for (int i = 0; i < NP_LOGO_DOTS; i++) {
        const np_logo_dot_t *d = &np_logo_dots[i];
        lv_obj_t *dot = lv_obj_create(scr);
        lv_obj_set_size(dot, NP_LOGO_R * 2, NP_LOGO_R * 2);
        lv_obj_set_pos(dot, LOGO_X + d->cx - NP_LOGO_R, LOGO_Y + d->cy - NP_LOGO_R);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(UI_TEXT), 0);
        // 官方文件里背景那一组是 opacity: 0.4,照搬。
        lv_obj_set_style_bg_opa(dot, d->faded ? LV_OPA_40 : LV_OPA_COVER, 0);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_clickable(dot, false);
    }
}

void np_ui_show_status(const char *line1, const char *line2, const char *line3,
                       const char *line4, const char *line5)
{
    fonts_init();
    if (s_status_scr) lv_obj_delete_async(s_status_scr);
    s_status_scr = mk_screen();
    s_status_hint = NULL;
    s_hint_text[0] = '\0';   // 整页重建等于换了个话题,上一轮的临时提示不带过来

    status_logo(s_status_scr);
    // 标题两行:20 px 的名称 + 14 px 的版本号。240 px 宽摆不下一行,而刷完
    // 不用连串口就能确认设备上跑的是哪一版。
    lv_obj_t *name = mk_label(s_status_scr, TITLE_X, 2, UI_TEXT, &np_font_20);
    lv_label_set_text(name, "Tailscale 监控牌");
    char ttl[40];
    const char *raw = esp_app_get_description()->version;
    // "np-" 是构建标识(bump_version.sh 与 check_embedded_version 都按它认),
    // 对屏幕前的人没有信息量,显示时摘掉。
    if (strncmp(raw, "np-", 3) == 0) raw += 3;
    snprintf(ttl, sizeof(ttl), "v%.24s", raw);
    lv_obj_t *ver = mk_label(s_status_scr, TITLE_X, 28, UI_MUTED, &s_font14);
    lv_label_set_text(ver, ttl);

    // 正文五行行距 18 px(14 px 字体行高 17):标题多出一行,正文跟着下移。
    // 最后一行 120+17=137 仍在按钮 146 之上。
    static const int ys[5] = { 48, 66, 84, 102, 120 };
    const char *lines[5] = { line1, line2, line3, line4, line5 };
    for (int i = 0; i < 5; i++) {
        lv_obj_t *lb = mk_label(s_status_scr, 14, ys[i], UI_TEXT, &s_font14);
        lv_obj_set_width(lb, 212);
        lv_label_set_long_mode(lb, LV_LABEL_LONG_DOT);
        lv_label_set_text(lb, lines[i] && lines[i][0] ? lines[i] : "-");
    }

    static const char *ACTS[NP_MENU_COUNT] = {
        "立即刷新", "局域网配置", "引导热点配网", "清除凭据" };
    for (int i = 0; i < NP_MENU_COUNT; i++) {
        lv_obj_t *btn = lv_button_create(s_status_scr);
        lv_obj_set_pos(btn, 12, 146 + i * 36);
        lv_obj_set_size(btn, 216, 30);
        lv_obj_set_style_bg_color(btn, lv_color_hex(UI_CARD), 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_border_width(btn, 2, 0);
        lv_obj_t *lb = lv_label_create(btn);
        lv_obj_set_style_text_font(lb, &s_font14, 0);
        lv_obj_set_style_text_color(lb, lv_color_hex(UI_TEXT), 0);
        lv_label_set_text(lb, ACTS[i]);
        lv_obj_center(lb);
        s_actions[i] = btn;
    }
    // 选中项跨重建保留:配对码页、清除提示都会重画这一屏,把光标弹回 0 号就等于
    // 按错了键。越界部分夹紧(设备数变少时同理)。
    s_action_sel = np_menu_clamp(s_action_sel);
    paint_actions();

    s_status_hint = mk_label(s_status_scr, 12, 292, UI_MUTED, &s_font14);
    lv_obj_set_width(s_status_hint, 216);
    lv_label_set_long_mode(s_status_hint, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_status_hint, "上下选择 确定执行 长按返回");

    lv_screen_load(s_status_scr);
    s_view = NP_VIEW_STATUS;
}

// ------------------------------------------------------------- 系统设置
typedef enum {
    SET_MAIN = 0,
    SET_BRIGHTNESS,
    SET_INTERVAL,
    SET_AOD,
    SET_NETWORK,
} settings_mode_t;
static settings_mode_t s_settings_mode;

#define SET_ROW_COUNT 4

static const char *SET_ROW_NAMES[SET_ROW_COUNT] = {
    "屏幕亮度", "自动熄屏", "熄屏显示", "网络配置",
};

static void settings_hint(const char *t)
{
    lv_obj_t *h = mk_label(s_settings_scr, 12, 292, UI_MUTED, &s_font14);
    lv_obj_set_width(h, 216);
    lv_label_set_long_mode(h, LV_LABEL_LONG_DOT);
    lv_label_set_text(h, t ? t : "上下选择 确定执行 长按返回");
}

// 主菜单各行的当前值:亮度百分比、熄屏间隔、开关;网络配置行无值。
static void settings_main_values(char values[SET_ROW_COUNT][24])
{
    np_display_cfg_off_label(app_cfg_off_minutes(), values[1], 24);
    snprintf(values[0], 24, "%u%%", (unsigned)app_cfg_brightness());
    snprintf(values[2], 24, "%s", app_cfg_aod_enabled() ? "开" : "关");
    values[3][0] = '\0';
}

// 整屏重绘(除亮度子模式只更新数值标签外)。调用方须持有 LVGL 锁。
static void paint_settings(void)
{
    if (!s_settings_scr) return;
    lv_obj_clean(s_settings_scr);

    if (s_settings_mode == SET_BRIGHTNESS) {
        lv_obj_t *title = mk_label(s_settings_scr, 14, 8, UI_TEXT, &s_font20);
        lv_label_set_text(title, "屏幕亮度");
        char val[16];
        snprintf(val, sizeof(val), "%u%%", (unsigned)app_cfg_brightness());
        s_brightness_val = mk_label(s_settings_scr, 14, 110, UI_ACCENT, &s_font20);
        lv_label_set_text(s_brightness_val, val);
        settings_hint("上下调节 长按返回");
        return;
    }

    if (s_settings_mode == SET_INTERVAL) {
        lv_obj_t *title = mk_label(s_settings_scr, 14, 8, UI_TEXT, &s_font20);
        lv_label_set_text(title, "自动熄屏");
        for (size_t i = 0; i < NP_DISPLAY_OFF_OPTIONS_COUNT; i++) {
            lv_obj_t *row = lv_obj_create(s_settings_scr);
            lv_obj_set_pos(row, 12, 60 + (int)i * 32);
            lv_obj_set_size(row, 216, 28);
            const bool sel = i == s_interval_sel;
            lv_obj_set_style_bg_color(row, lv_color_hex(sel ? UI_CARD_SEL : UI_CARD), 0);
            lv_obj_set_style_border_width(row, 2, 0);
            lv_obj_set_style_border_color(row,
                lv_color_hex(sel ? UI_ACCENT : UI_ROW_LINE), 0);
            lv_obj_set_style_radius(row, 8, 0);
            char label[16];
            np_display_cfg_off_label(np_display_off_options[i], label, sizeof(label));
            lv_obj_t *lb = lv_label_create(row);
            lv_obj_set_style_text_font(lb, &s_font14, 0);
            lv_obj_set_style_text_color(lb, lv_color_hex(sel ? UI_TEXT : UI_MUTED), 0);
            lv_label_set_text(lb, label);
            lv_obj_center(lb);
        }
        settings_hint("上下选择 确定执行 长按返回");
        return;
    }

    if (s_settings_mode == SET_AOD) {
        lv_obj_t *title = mk_label(s_settings_scr, 14, 8, UI_TEXT, &s_font20);
        lv_label_set_text(title, "熄屏显示");
        char val[40];
        snprintf(val, sizeof(val), "熄屏时显示时间与在线数:%s",
                 app_cfg_aod_enabled() ? "开" : "关");
        lv_obj_t *lb = mk_label(s_settings_scr, 14, 110, UI_TEXT, &s_font14);
        lv_obj_set_width(lb, 212);
        lv_label_set_long_mode(lb, LV_LABEL_LONG_DOT);
        lv_label_set_text(lb, val);
        settings_hint("确定切换 长按返回");
        return;
    }

    if (s_settings_mode == SET_NETWORK) {
        lv_obj_t *title = mk_label(s_settings_scr, 14, 8, UI_TEXT, &s_font20);
        lv_label_set_text(title, "网络配置");
        // 文案与状态页菜单保持同一份契约(np_menu_activate 按下标分发)。
        static const char *ACTS[NP_MENU_COUNT] = {
            "立即刷新", "局域网配置", "引导热点配网", "清除凭据" };
        for (size_t i = 0; i < NP_MENU_COUNT; i++) {
            lv_obj_t *row = lv_obj_create(s_settings_scr);
            lv_obj_set_pos(row, 12, 60 + (int)i * 40);
            lv_obj_set_size(row, 216, 32);
            const bool sel = i == s_net_sel;
            lv_obj_set_style_bg_color(row, lv_color_hex(sel ? UI_CARD_SEL : UI_CARD), 0);
            lv_obj_set_style_border_width(row, 2, 0);
            lv_obj_set_style_border_color(row,
                lv_color_hex(sel ? UI_ACCENT : UI_ROW_LINE), 0);
            lv_obj_set_style_radius(row, 8, 0);
            lv_obj_t *lb = lv_label_create(row);
            lv_obj_set_style_text_font(lb, &s_font14, 0);
            lv_obj_set_style_text_color(lb, lv_color_hex(sel ? UI_TEXT : UI_MUTED), 0);
            lv_label_set_text(lb, ACTS[i]);
            lv_obj_center(lb);
        }
        settings_hint("上下选择 确定执行 长按返回");
        return;
    }

    lv_obj_t *title = mk_label(s_settings_scr, 14, 8, UI_TEXT, &s_font20);
    lv_label_set_text(title, "系统设置");
    char values[SET_ROW_COUNT][24];
    settings_main_values(values);
    for (int i = 0; i < SET_ROW_COUNT; i++) {
        lv_obj_t *row = lv_obj_create(s_settings_scr);
        lv_obj_set_pos(row, 12, 60 + i * 40);
        lv_obj_set_size(row, 216, 32);
        const bool sel = (size_t)i == s_setting_sel;
        lv_obj_set_style_bg_color(row, lv_color_hex(sel ? UI_CARD_SEL : UI_CARD), 0);
        lv_obj_set_style_border_width(row, 2, 0);
        lv_obj_set_style_border_color(row,
            lv_color_hex(sel ? UI_ACCENT : UI_ROW_LINE), 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_t *nm = lv_label_create(row);
        lv_obj_set_style_text_font(nm, &s_font14, 0);
        lv_obj_set_style_text_color(nm, lv_color_hex(UI_TEXT), 0);
        lv_label_set_text(nm, SET_ROW_NAMES[i]);
        lv_obj_align(nm, LV_ALIGN_LEFT_MID, 12, 0);
        lv_obj_t *vl = lv_label_create(row);
        lv_obj_set_style_text_font(vl, &s_font14, 0);
        lv_obj_set_style_text_color(vl, lv_color_hex(sel ? UI_TEXT : UI_MUTED), 0);
        lv_label_set_text(vl, values[i]);
        lv_obj_align(vl, LV_ALIGN_RIGHT_MID, -12, 0);
    }
    settings_hint(NULL);
}

void np_ui_settings_enter(void)
{
    fonts_init();
    if (s_settings_scr) lv_obj_delete_async(s_settings_scr);
    s_settings_scr = mk_screen();
    s_settings_mode = SET_MAIN;
    s_setting_sel = 0;
    s_interval_sel = 0;
    s_net_sel = 0;
    paint_settings();
    lv_screen_load(s_settings_scr);
    s_view = NP_VIEW_SETTINGS;
}

// 亮度调节只改内存,离开设置统一落盘,避免 NVS 频繁擦写。
void np_ui_settings_save_and_back(void)
{
    (void)app_cfg_save();
    if (s_view == NP_VIEW_SETTINGS) {
        lv_screen_load(s_list_scr);
        s_view = NP_VIEW_LIST;
    }
}

// ------------------------------------------------------------- 熄屏显示
static void aod_build(void)
{
    if (s_aod_scr) return;
    s_aod_scr = mk_screen();
    // 标签不设固定宽度、文本自适应,LV_ALIGN_CENTER 才会真正水平居中:
// 设宽 240 加左对齐会把文字推到右边,用户实测看起来不居中。
    s_aod_time = mk_label(s_aod_scr, 0, 0, UI_TEXT, &lv_font_montserrat_48);
    lv_label_set_text(s_aod_time, "--:--");
    lv_obj_align(s_aod_time, LV_ALIGN_CENTER, 0, -40);
    // 设备数 x/y:x = 在线数,y = 总数;比 np-0.2.9 再大两档(Montserrat 28)。
    s_aod_online = mk_label(s_aod_scr, 0, 0, UI_MUTED, &lv_font_montserrat_28);
    lv_label_set_text(s_aod_online, "0/0");
    lv_obj_align(s_aod_online, LV_ALIGN_CENTER, 0, 48);
}

// 设备数 x/y 的显示与布局:has_count 为 false(还没成功读到主机列表)时隐藏
// 设备数、时间居中;为 true 时显示 x/y、时间上移。调用方须持有 LVGL 锁。
static void aod_set_count(bool has_count, uint32_t online, uint32_t total)
{
    if (!s_aod_scr) return;
    if (has_count) {
        lv_obj_remove_flag(s_aod_online, LV_OBJ_FLAG_HIDDEN);
        char buf[16];
        snprintf(buf, sizeof(buf), "%u/%u", (unsigned)online, (unsigned)total);
        lv_label_set_text(s_aod_online, buf);
        lv_obj_align(s_aod_online, LV_ALIGN_CENTER, 0, 48);
        lv_obj_align(s_aod_time, LV_ALIGN_CENTER, 0, -40);
    } else {
        lv_obj_add_flag(s_aod_online, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(s_aod_time, LV_ALIGN_CENTER, 0, 0);
    }
}

void np_ui_aod_show(const char *hm, uint32_t online, uint32_t total, bool has_count)
{
    fonts_init();
    aod_build();
    if (!s_aod_scr) return;
    if (hm && hm[0]) lv_label_set_text(s_aod_time, hm);
    aod_set_count(has_count, online, total);
    lv_screen_load(s_aod_scr);
    s_view = NP_VIEW_AOD;
}

void np_ui_aod_update(const char *hm, uint32_t online, uint32_t total, bool has_count)
{
    if (s_view != NP_VIEW_AOD || !s_aod_scr) return;
    if (hm && hm[0]) lv_label_set_text(s_aod_time, hm);
    aod_set_count(has_count, online, total);
}

// ------------------------------------------------------------- 按键
np_view_t np_ui_view(void) { return s_view; }
size_t np_ui_selected(void) { return s_selected; }

np_key_result_t np_ui_handle_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK && ev != BSP_BTN_LONG) return NP_KEY_NONE;

    if (ev == BSP_BTN_LONG && btn == BSP_BTN_OK) {
        // 主界面长按确定直接进设置;状态页入口仍在详情页按确定。
        if (s_view == NP_VIEW_LIST) return NP_KEY_OPEN_SETTINGS;
        if (s_view == NP_VIEW_DETAIL || s_view == NP_VIEW_STATUS) {
            lv_screen_load(s_list_scr);
            s_view = NP_VIEW_LIST;
            return NP_KEY_BACK_LIST;
        }
        if (s_view == NP_VIEW_SETTINGS) {
            if (s_settings_mode == SET_MAIN) {
                np_ui_settings_save_and_back();
                return NP_KEY_BACK_LIST;
            }
            s_settings_mode = SET_MAIN;   // 子模式长按:不落盘,回主菜单
            paint_settings();
            return NP_KEY_NONE;
        }
        if (s_view == NP_VIEW_AOD) {
            lv_screen_load(s_list_scr);
            s_view = NP_VIEW_LIST;
            return NP_KEY_BACK_LIST;
        }
        return NP_KEY_NONE;
    }
    if (ev != BSP_BTN_CLICK) return NP_KEY_NONE;

    switch (s_view) {
    case NP_VIEW_LIST:
        if (btn == BSP_BTN_UP && s_selected > 0) {
            s_selected--;
            np_ui_highlight();
        } else if (btn == BSP_BTN_DOWN && s_selected + 1u < NP_MAX_DEVICES) {
            s_selected++; // 越界部分由 render 夹紧到有效范围
            np_ui_highlight();
        } else if (btn == BSP_BTN_OK) {
            return NP_KEY_OPEN_DETAIL;
        }
        break;
    case NP_VIEW_DETAIL:
        if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
            // 上下键在详情里只管滚动:到顶或到底就停住,退出只有长按确定。
            // 读中间几行时按错方向就把整页收掉,代价和一次按键完全不匹配。
            if (!s_detail_body) break;
            const bool down = btn == BSP_BTN_DOWN;
            int32_t to = lv_obj_get_scroll_y(s_detail_body) +
                         (down ? ROW_STEP : -ROW_STEP);
            if (to < 0) to = 0;
            lv_obj_scroll_to_y(s_detail_body, to, LV_ANIM_OFF);
        } else if (btn == BSP_BTN_OK) {
            return NP_KEY_OPEN_STATUS;
        }
        break;
    case NP_VIEW_STATUS:
        if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
            s_action_sel = np_menu_move(s_action_sel, btn == BSP_BTN_DOWN ? 1 : -1);
            paint_actions();
        } else if (btn == BSP_BTN_OK) {
            switch (np_menu_activate(s_action_sel)) {
            case NP_MENU_SYNC: return NP_KEY_ACTION_SYNC;
            case NP_MENU_LAN:  return NP_KEY_ACTION_PROV;
            case NP_MENU_AP:   return NP_KEY_ACTION_AP;
            default:           return NP_KEY_ACTION_CLEAR;
            }
        }
        break;
    case NP_VIEW_SETTINGS:
        if (s_settings_mode == SET_MAIN) {
            if (btn == BSP_BTN_UP && s_setting_sel > 0) {
                s_setting_sel--;
                paint_settings();
            } else if (btn == BSP_BTN_DOWN && s_setting_sel + 1u < SET_ROW_COUNT) {
                s_setting_sel++;
                paint_settings();
            } else if (btn == BSP_BTN_OK) {
                if (s_setting_sel == 0)      s_settings_mode = SET_BRIGHTNESS;
                else if (s_setting_sel == 1) s_settings_mode = SET_INTERVAL;
                else if (s_setting_sel == 2) s_settings_mode = SET_AOD;
                else                         s_settings_mode = SET_NETWORK;
                paint_settings();
            }
        } else if (s_settings_mode == SET_BRIGHTNESS) {
            if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
                const int step = (btn == BSP_BTN_UP) ? 5 : -5;
                const uint8_t v = np_display_cfg_clamp_brightness(
                                      (int)app_cfg_brightness() + step);
                (void)app_cfg_set_brightness(v);
                bsp_display_backlight(v);   // 实时生效
                if (s_brightness_val) {
                    char val[16];
                    snprintf(val, sizeof(val), "%u%%", (unsigned)v);
                    lv_label_set_text(s_brightness_val, val);
                }
            }
        } else if (s_settings_mode == SET_INTERVAL) {
            if (btn == BSP_BTN_UP && s_interval_sel > 0) {
                s_interval_sel--;
                paint_settings();
            } else if (btn == BSP_BTN_DOWN &&
                       s_interval_sel + 1u < NP_DISPLAY_OFF_OPTIONS_COUNT) {
                s_interval_sel++;
                paint_settings();
            } else if (btn == BSP_BTN_OK) {
                (void)app_cfg_set_off_minutes(np_display_off_options[s_interval_sel]);
                s_settings_mode = SET_MAIN;
                paint_settings();
            }
        } else if (s_settings_mode == SET_NETWORK) {
            if (btn == BSP_BTN_UP && s_net_sel > 0) {
                s_net_sel--;
                paint_settings();
            } else if (btn == BSP_BTN_DOWN && s_net_sel + 1u < NP_MENU_COUNT) {
                s_net_sel++;
                paint_settings();
            } else if (btn == BSP_BTN_OK) {
                switch (np_menu_activate(s_net_sel)) {
                case NP_MENU_SYNC: return NP_KEY_ACTION_SYNC;
                case NP_MENU_LAN:  return NP_KEY_ACTION_PROV;
                case NP_MENU_AP:   return NP_KEY_ACTION_AP;
                default:           return NP_KEY_ACTION_CLEAR;
                }
            }
        } else {   // SET_AOD
            if (btn == BSP_BTN_OK) {
                (void)app_cfg_set_aod_enabled(!app_cfg_aod_enabled());
                paint_settings();
            }
        }
        break;
    case NP_VIEW_AOD:
        // 熄屏显示时任意键唤醒:背光恢复由 app 侧做,这里只切回列表。
        lv_screen_load(s_list_scr);
        s_view = NP_VIEW_LIST;
        return NP_KEY_BACK_LIST;
    }
    return NP_KEY_NONE;
}
