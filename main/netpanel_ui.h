// main/netpanel_ui.h —— 网络面板 LVGL 界面:列表 / 详情 / 系统状态三视图。
// 全部函数只允许在 UI 任务(或持 bsp_lvgl_lock 的调用)里执行。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bsp_button.h"
#include "np_menu.h"
#include "np_model.h"

typedef enum {
    NP_VIEW_LIST = 0,
    NP_VIEW_DETAIL,
    NP_VIEW_STATUS,
    NP_VIEW_SETTINGS,   // 系统设置:亮度/熄屏间隔/熄屏显示
    NP_VIEW_AOD,        // 熄屏显示:时间 + 在线设备数(低亮度常显)
} np_view_t;

// 列表视图:建屏并挂上。app 启动/返回时调用一次。
void np_ui_list_enter(void);
void np_ui_list_exit(void);

// 数据/状态刷新(整行重绘)。err_text 是故障(红字"异常:…"),note_text 是当前
// 在等什么(等校时/等地址这类,青字、不冠"异常"),两者都空才显示常规操作提示行。
// sync_epoch 是手上这份快照换上的时刻(墙上钟秒,0 = 本轮还没同步成功),
// 常规页脚会写"同步 HH:MM";钟没校上时调用方传 0。
// fresh = 这份快照还描述"现在"(见 np_snapshot_fresh):false 时行名退到次要色、绿点画成
// 空心圈 —— 数据里的 online 是抓取那一刻的事实,链路断了就不能再当成现在。
void np_ui_render_list(const np_list_t *data, bool time_synced, int64_t now,
                       int32_t tz_offset_s, int battery_pct, int8_t rssi,
                       const char *err_text, const char *note_text,
                       int64_t sync_epoch, bool fresh);

// 详情视图:展示 data 中第 idx 台设备(越界则回列表)。
// det 是另拉的扩展信息(路由/DERP/发行版),NULL 表示还没到;det_note 是这一行的
// 进度或失败原因,显示在原操作提示的位置。fresh 同 np_ui_render_list。
void np_ui_show_detail(const np_list_t *data, size_t idx, bool time_synced,
                       int64_t now, int32_t tz_offset_s,
                       const np_detail_t *det, const char *det_note,
                       bool fresh);

// 系统状态视图(轮询节奏、凭据摘要、四项菜单)。正文五行,line4 是账号、line5 是
// 登录状态;配网页只用到 line5,那里写的是这一页的按键说明,其余传空串占位。
// 菜单选中项跨重建保留。
void np_ui_show_status(const char *line1, const char *line2, const char *line3,
                       const char *line4, const char *line5);

// 只改写状态页底部的提示行,不重建整页(两次确认这类临时话术要原地换)。
// 传 NULL 或空串退回默认文案;状态页没开着时不动作。调用方须持有 LVGL 锁。
void np_ui_status_hint(const char *text);

// 按键分发(UI 任务内)。返回按键造成的导航意图,由 app 决定后续渲染。
typedef enum {
    NP_KEY_NONE = 0,       // 已就地处理(选中高亮等)
    NP_KEY_OPEN_DETAIL,    // 请求展示 selected 的详情
    NP_KEY_OPEN_STATUS,    // 请求切到系统状态视图
    NP_KEY_OPEN_SETTINGS,  // 请求切到系统设置视图
    NP_KEY_BACK_LIST,      // 请求回到列表(需重绘)
    NP_KEY_ACTION_SYNC,    // 立即刷新
    NP_KEY_ACTION_PROV,    // 打开局域网配置服务
    NP_KEY_ACTION_AP,      // 起引导热点配网
    NP_KEY_ACTION_CLEAR,   // 清除 Tailscale 凭据
} np_key_result_t;

np_key_result_t np_ui_handle_key(bsp_btn_t btn, bsp_btn_ev_t ev);
np_view_t np_ui_view(void);
size_t np_ui_selected(void);
// 列表视图重绘后恢复选中态(app 调完 render 后补一次)。
void np_ui_highlight(void);

// 系统设置:建主菜单屏并切过去。调用方须持有 LVGL 锁。
void np_ui_settings_enter(void);
// 设置屏长按返回:落盘并切回列表屏。调用方须持有 LVGL 锁。
void np_ui_settings_save_and_back(void);
// 熄屏显示:建/复用 AOD 屏并切过去,内容为时间 + 设备数 x/y(在线/总数)。
// has_count 为 false(还没成功读到 Tailscale 主机列表)时只居中显示时间,不显示
// x/y,避免在数据没到手前报出不准的在线状况。调用方须持有 LVGL 锁。
void np_ui_aod_show(const char *hm, uint32_t online, uint32_t total, bool has_count);
// 刷新已显示的 AOD 屏内容(未显示时为空操作)。调用方须持有 LVGL 锁。
void np_ui_aod_update(const char *hm, uint32_t online, uint32_t total, bool has_count);
