// main/netpanel_app.c —— 双任务:app 任务(状态机/按键/UI 渲染)+ poll 任务(HTTPS)。
// 数据交接:poll 写入备用快照后加 LVGL 锁换入活动快照;UI 只读活动快照。
#include "netpanel_app.h"

#include "app_cfg.h"
#include "app_prov.h"
#include "app_wifi.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"  // 只用 bsp_lvgl_lock/unlock;亮度不再由本应用干预
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "netpanel_ui.h"
#include "np_account.h"
#include "np_clock.h"
#include "np_display_cfg.h"
#include "np_errfmt.h"
#include "np_mem.h"
#include "np_model.h"
#include "np_prompt.h"
#include "np_time.h"
#include "ts_api.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "np_app";

typedef enum { ST_BOOT, ST_WIFI_RETRY, ST_RUN, ST_PROV } app_state_t;

typedef enum { EV_KEY, EV_RENDER } ev_type_t;

typedef struct {
    ev_type_t type;
    bsp_btn_t btn;
    bsp_btn_ev_t ev;
    uint32_t posted_ms; // 写进队列那一刻的单调毫秒(取低 32 位,只用来量延迟)
    uint8_t retry;      // 因为拿不到渲染锁而被再发了一次、两次
} app_ev_t;

#define UI_ERR_CAP 48
#define UI_ERR_DETAIL_CAP 80
// 配置页(配对码页/热点页)只认长按确定:上下键在这页没有可移动的东西。
// 这句话是这一页唯一的说明书,画这一页的两处必须说同一句,所以写成一处。
static const char PROV_EXIT_HINT[] = "长按确定退出本页";
// 热点页长按确定不"退出"去哪一页:没有凭据时这页就是设备能给的下一页,长按
// 等于把热点重开一轮。写"退出本页"会让人以为按完能走。
static const char PROV_AP_HINT[] = "长按确定重开热点";
// 校时等待:证书校验要真实时间,而钟没校上时 HTTPS 必然撞在"有效期还没开始"
// 的证书上 —— 那种失败不可能成功,所以请求是真的等到校上为止才发,不再"过了
// 宽限期就照常发起"(np-0.1.22 实机:提交凭据后仍然报证书异常,重试三轮不过是
// 把同一件必然失败的事做三遍)。等多久重起一轮 SNTP、等多久改口说实话,都在
// np_clock.h。

static QueueHandle_t s_queue;
static app_state_t s_state;
static np_poll_t s_poll;
static char s_err[UI_ERR_CAP];
static char s_note[UI_ERR_CAP];
static char s_err_detail[UI_ERR_DETAIL_CAP]; // 状态页第二行:数字细节
static bool s_auth_halted;
// 最近一次同步的结果:0 = 本次开机还没试过(或换了凭据,旧的成败不作数),
// 1 = 成功,-1 = 失败。状态页的"上次同步"和"登录"两行都由它说话。
static int8_t s_last_sync;
// 活动快照是哪一刻换上的(单调毫秒,0 = 本轮开机还没同步成功过)。写只在 poll
// 任务换快照那一步、且在 LVGL 锁里;读都在渲染那一步、也在同一把锁里。
static int64_t s_snapshot_ms;
// 同一件事的墙上钟版本,页脚"同步 HH:MM"用:esp_timer 的单调毫秒换算不成
// 人看的时刻。与 s_snapshot_ms 同锁写、同锁读。
static int64_t s_snapshot_wall;
static bool s_time_synced;
static np_clock_t s_clock;     // 校时门禁的等待/重起节奏
// 屏幕上那行"正在等什么"由谁说的:等待的理由(断网/等校时/校时无响应)只在该
// 说的那一刻说一次,轮询节拍 500 ms,照着刷会把一句话刷成闪烁。别的 set_note
//  caller(登录提示之类)写这一行时把登记退回 NONE,于是真在等什么时能重新顶掉它。
typedef enum {
    NOTE_NONE = 0,
    NOTE_LINK_DOWN,
    NOTE_CLOCK_WAIT,
    NOTE_CLOCK_STALL,
} note_kind_t;
static note_kind_t s_note_kind;
static int s_battery = -1;
static volatile bool s_link_seen;  // 本次开机拿到过 IPv4(没拿到过就还在连接流程里)
static int64_t s_clear_prompt_ms;
static bool s_prompt_dismissed; // 本次开机还弹不弹"补填凭据"提示
// 熄屏:最后活动时刻(单调毫秒)、是否已熄屏、是否停在 AOD 页。
static int64_t s_last_activity_ms;
static bool s_screen_off;
static bool s_aod_active;
static char s_prov_a[40], s_prov_b[40];
static bool s_prov_ap; // 当前/上一次配置会话是不是引导热点
static np_prompt_t s_prov_lines;   // 配对码页正显示的四行
static char s_prov_ip[16];         // 这四行是按哪个地址拼的,变了要重画

// 详情页的扩展信息(路由/DERP/发行版)要单独再拉一次 HTTPS。请求和结果都只在
// LVGL 锁里交接:和列表快照同一套规矩,省掉一套自己的内存顺序约定。
#define DETAIL_NOTE_CAP 64
static np_detail_t s_detail;
static char s_detail_note[DETAIL_NOTE_CAP];
static char s_detail_id[NP_ID_LEN];
static size_t s_detail_idx;      // 详情当前展示的是第几行
static uint32_t s_detail_gen;    // 世代号:换机器后旧响应作废
static bool s_detail_req;        // 待 poll 任务取走
static bool s_detail_ok;         // s_detail 里是真数据

// ---------------------------------------------------------------- 状态通道
// 故障(s_err)与"正在等什么"(s_note)分开:等校时、等地址这些是过程不是毛病,
// 让它们走上屏的"异常"行,用户就会去找一个并不存在的故障。写一个清另一个。
static void set_err(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_err, sizeof(s_err), fmt, ap);
    va_end(ap);
    s_note[0] = '\0';
}

static void set_note(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_note, sizeof(s_note), fmt, ap);
    va_end(ap);
    s_err[0] = '\0';
    s_err_detail[0] = '\0';
    s_note_kind = NOTE_NONE;   // 这一行换人说了,等待那套逻辑要能重新接上话
}

static void clear_status(void)
{
    s_err[0] = '\0';
    s_note[0] = '\0';
    s_err_detail[0] = '\0';
}

static uint32_t now_ms32(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static bool post_ev(const app_ev_t *e)
{
    if (!s_queue || xQueueSend(s_queue, e, 0) != pdTRUE) {
        ESP_LOGW(TAG, "事件队列已满,丢弃 type=%d", (int)e->type);
        return false;
    }
    return true;
}

static bool post(ev_type_t t, bsp_btn_t btn, bsp_btn_ev_t ev)
{
    const app_ev_t e = { .type = t, .btn = btn, .ev = ev, .posted_ms = now_ms32() };
    return post_ev(&e);
}

// 屏幕上"正在等什么"那一行,理由会变(断网 / 等校时 / 校时压根没回应),而每种
// 理由只在该说的那一刻说一次:轮询节拍是 500 ms,照着刷会把一句话刷成闪烁。
// 记的是"上一拍说的是哪种",不是"这句话刷过没有" —— 换理由时还要再刷一次。
// 断网那一条是唯一例外:它带着补发的倒计时,数字每秒都在变,内容变了就要重画。
static void set_wait_note(note_kind_t kind)
{
    if (kind == NOTE_LINK_DOWN) {
        // 断网不是同步失败:报"连不上服务器"会把人支到路由器外面去找原因。
        // 后半句说的是阶梯下一次补发还要等多久:页脚在动,人就看得出有人在管,
        // 不会把等待当成死机 —— 用户报的正是"说 Wi-Fi 连不上又没人在修"。
        char line[UI_ERR_CAP];
        const uint32_t left = app_wifi_retry_countdown_ms();
        if (left != 0u) {
            snprintf(line, sizeof(line), "Wi-Fi 已断,重连中下次 %lu 秒后",
                     (unsigned long)((left + 999u) / 1000u));
        } else {
            strlcpy(line, "Wi-Fi 已断,重连后自动继续", sizeof(line));
        }
        // 倒计时每秒换一个数,这一条不参与"同一句话只说一次":数字没变才不重画。
        if (strcmp(s_note, line) == 0 && s_note_kind == kind) return;
        set_note("%s", line);
        s_note_kind = kind;
        (void)post(EV_RENDER, BSP_BTN_UP, BSP_BTN_CLICK);
        return;
    }
    if (s_note_kind == kind) return;
    switch (kind) {
    case NOTE_LINK_DOWN: // 上面单独处理过了:它带倒计时,不走"说过就不再提"
        return;
    case NOTE_CLOCK_WAIT:
        set_note("等待校时,SNTP 还没回来");
        break;
    case NOTE_CLOCK_STALL:
        // 等了两分钟还没校上,该背锅的不是证书:这句话说的是路由器那一侧。
        set_note("校时无响应:路由器可能封了 UDP 123");
        break;
    case NOTE_NONE:
        // 等待的原因是临时的:理由消失就撤掉那行青字,别让它赖在屏上变成一条
        // 谁也说不清何时结束的"正在等待"。真故障(s_err)不归这里管。
        if (!s_err[0]) s_note[0] = '\0';
        break;
    }
    s_note_kind = kind;   // 放在 set_note 之后:它把登记退回 NONE 才能重新接上话
    (void)post(EV_RENDER, BSP_BTN_UP, BSP_BTN_CLICK);
}

bool np_app_key_event(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    return post(EV_KEY, btn, ev);
}

// ---------------------------------------------------------------- UI 渲染
// 快照是不是还描述"现在":链路断了就立刻不算(这就是"页脚说已断、点还绿着"
// 那件事的解药),链路在则按这一轮同步自己的窗口算,退避到上限不算过期。
static bool snapshot_fresh(void)
{
    return np_snapshot_fresh(esp_timer_get_time() / 1000, s_snapshot_ms,
                             app_wifi_has_ip(), s_poll.current_delay_s);
}

static void render_now(void)
{
    if (!bsp_lvgl_lock(500)) return;
    const int64_t now = (int64_t)time(NULL);
    if (np_ui_view() == NP_VIEW_LIST) {
        np_ui_render_list(np_lists_active(), s_time_synced, now,
                          app_cfg_tz_offset_s(),
                          s_battery, app_wifi_rssi(), s_err, s_note,
                          s_snapshot_wall, snapshot_fresh());
    }
    bsp_lvgl_unlock();
}

// 配对码页复用状态屏的正文五行。第五行是这一页的按键说明:这一页按设计只认
// 长按确定(上下键在这页没有可移动的东西),不写出来就没人知道它没坏。
static void show_prov_screen(const char *l1, const char *l2, const char *l3,
                             const char *l4, const char *l5)
{
    if (!bsp_lvgl_lock(1000)) return;
    np_ui_show_status(l1, l2, l3, l4, l5);
    bsp_lvgl_unlock();
}

// 回列表必须重建列表屏:np_ui_list_exit() 把列表屏连同配网页一起删了,此后
// np_ui_render_list() 见 s_list_scr 为空直接返回,画面就停在最后一帧配网页。
static void reenter_list(void)
{
    if (!bsp_lvgl_lock(1000)) return;
    np_ui_list_exit();
    np_ui_list_enter();
    bsp_lvgl_unlock();
    render_now();
}

// ---------------------------------------------------------------- 详情页扩展信息
// 只画屏:det 为 NULL 时扩展行显示 "-",det_note 落在原操作提示的位置。
// 调用方须持有 LVGL 锁。
static void paint_detail(void)
{
    np_ui_show_detail(np_lists_active(), s_detail_idx, s_time_synced,
                      (int64_t)time(NULL), app_cfg_tz_offset_s(),
                      s_detail_ok ? &s_detail : NULL, s_detail_note,
                      snapshot_fresh());
}

// 打开详情:先把已有画面推上去,再挂一个扩展信息请求给 poll 任务。
// HTTPS 不能在 UI 任务里跑(栈要为渲染留量),所以这里只交接请求。
// 返回 false = 这一行不存在(列表为空),调用方改用状态页兜底。
static bool open_detail(size_t idx)
{
    if (!bsp_lvgl_lock(500)) return false;
    const np_list_t *snap = np_lists_active();   // 读快照必须在锁内
    if (idx >= snap->count) { bsp_lvgl_unlock(); return false; }
    const np_device_t *d = &snap->items[idx];
    s_detail_idx = idx;
    s_detail_gen++;
    s_detail_ok = false;
    np_detail_clear(&s_detail);
    s_detail_id[0] = '\0';
    if (d->id[0]) {
        strlcpy(s_detail_id, d->id, sizeof(s_detail_id));
        s_detail_req = true;
        snprintf(s_detail_note, sizeof(s_detail_note), "扩展信息获取中");
    } else {
        // 老快照或外部节点没给 id:没什么可拉的,直接说明。
        s_detail_req = false;
        snprintf(s_detail_note, sizeof(s_detail_note), "扩展信息:无节点 ID");
    }
    paint_detail();
    bsp_lvgl_unlock();
    return true;
}

// poll 任务看一眼有没有待处理的详情请求。这里只读不清:请求要等结果真的落上屏
// 才撤销,拿不到锁或世代对不上时下一拍还能重试。锁只用来交接,不在锁里做 HTTPS。
static bool peek_detail_request(char *id, size_t cap, uint32_t *gen)
{
    if (!bsp_lvgl_lock(200)) return false;
    const bool has = s_detail_req;
    if (has) {
        strlcpy(id, s_detail_id, cap);
        *gen = s_detail_gen;
    }
    bsp_lvgl_unlock();
    return has;
}

// ---------------------------------------------------------------- 网络动作
static bool clock_synced(void)
{
    // SNTP 校时成功前 time() 停在 1970 附近。
    return time(NULL) > 1700000000LL;
}

static void sntp_start(void)
{
    // 已经校上就别重开:esp_sntp_init() 会再建一个 UDP pcb,旧的没人收。
    if (clock_synced()) return;
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    // 三台而不是两台:任一台被路由拦掉、被 DNS 解析到不通的地址,这一轮还有别的可试。
    esp_sntp_setservername(0, "0.pool.ntp.org");
    esp_sntp_setservername(1, "1.pool.ntp.org");
    esp_sntp_setservername(2, "2.pool.ntp.org");
    esp_sntp_init();
}

// 重起一轮 SNTP。第一次交换完全可能落空:配置页的全信道扫描要借走射频、断链自愈
// 会重连,而 lwip 默认同步间隔是十分钟 —— 落空一次就白等十分钟。什么时候该再试
// 由 np_clock 按 NP_CLOCK_RETRY_MS 决定,这里只负责"再试一次"。
static void clock_rearm(void)
{
    esp_sntp_stop();   // 没起过时是空操作(lwip 里 pcb 为空直接返回)
    sntp_start();
}

// ts_api 的失败细节 → 人话原因。轮询失败与详情失败共用同一套措辞。
static void err_src(np_err_src_t *src)
{
    const ts_fail_t *f = ts_api_last_failure();
    src->http_status = f->http_status;
    src->esp_err = f->esp_err;
    src->tls_code = f->tls_code;
    src->tls_flags = f->tls_flags;
    src->sys_errno = f->sys_errno;
    src->free_heap = f->free_heap;
    src->free_blk = f->free_blk;
    src->host = f->host;
    src->msg = f->msg;
    // 请求只在钟真校上之后才发得出去(见 poll_task 的门禁),所以这里读到的就是
    // 发起那一刻的事实。旧版要先存一份快照,是因为宽限期一过就会带着 1970 年的
    // 钟出门,而握手失败到拼这行错误之间 SNTP 可能刚好回来,把"还没校时"说成
    // "证书有效期不对(校过时钟?)"——那句在这台设备上的言下之意是"没救了,重启吧"。
    src->clock_synced = clock_synced();
    src->transport = f->transport;
    src->too_big = f->too_big;
}

// 多网络记忆:连之前先看附近有哪些已记住的网,挑信号最强的连。只有一个
// 记忆时不值得花一两秒扫描(直连最近用的);扫描失败也不拦路,按最近用的
// 盲连 —— 扫描是优化,不该成为新的故障点。扫描成功但一张已记住的网都不
// 在附近,就明说,不烧满连接超时才落到重试页。
static bool wifi_pick_known(void)
{
    if (app_cfg_wifi_count() <= 1u) return true;
    np_scan_ap_t aps[NP_SCAN_KEEP];
    size_t n = 0;
    if (app_wifi_scan(NULL, aps, NP_SCAN_KEEP, &n) != ESP_OK) return true;
    const int pick = app_cfg_wifi_pick(aps, n);
    if (pick < 0) return false;
    (void)app_cfg_wifi_select((size_t)pick);
    return true;
}

static bool wifi_connect_and_run(void)
{
    s_time_synced = false;
    if (!wifi_pick_known()) {
        set_err("附近没有已记住的 Wi-Fi,长按重配");
        return false;
    }
    esp_err_t err = app_wifi_connect();
    if (err == ESP_OK) {
        s_link_seen = true;
        np_clock_init(&s_clock);   // 新一轮连接,SNTP 也是刚起的,旧账不带过来
        sntp_start();
        return true;
    }
    char tried[13];
    strlcpy(tried, app_cfg_ssid(), sizeof(tried));
    if (err == APP_ERR_WIFI_AUTH_FAIL) {
        set_err("Wi-Fi 密码错误:%s,长按重配", tried);
    } else if (err == APP_ERR_WIFI_NO_IP) {
        // 关联上了却没地址:多半是 AP 侧 DHCP 没响应,和"搜不到网络"分开报。
        set_err("%s 已连,未取得 IP,确定重试", tried);
    } else if (err == ESP_ERR_TIMEOUT) {
        set_err("Wi-Fi 连不上:%s,确定重试", tried);
    } else {
        set_err("Wi-Fi 启动失败,确定重试");
    }
    return false;
}

// 起引导热点。热点会话没有"在线"可言:结束后一律回 enter_boot 重新走连接。
// 会话是互斥的:上一轮没收尾就开新的一轮,app_prov_start 必然挡回
// INVALID_STATE,所以先停。开关会话只有 np_app 这一处,app_prov_stop() 是纯拆除。
static void start_ap_prov(void)
{
    app_prov_stop();
    app_wifi_stop();
    const esp_err_t prov_err = app_prov_start(
        NP_PROV_SOFTAP, s_prov_a, sizeof(s_prov_a), NULL, 0);
    if (prov_err != ESP_OK || !s_prov_a[0]) {
        // 把 esp_err_t 留在日志里,屏幕上只剩一句"启动失败",谁也不知道是被谁
        // 挡的。名字是纯 ASCII,不新增字形。
        char why[40];
        if (prov_err != ESP_OK) {
            ESP_LOGE(TAG, "热点会话启动失败: %s", esp_err_to_name(prov_err));
            snprintf(why, sizeof(why), "%s", esp_err_to_name(prov_err));
        } else {
            ESP_LOGE(TAG, "热点起来了但没读到热点名称");
            snprintf(why, sizeof(why), "no ssid name");
        }
        s_prov_ap = false;
        // 失败不是"会话开着":热点没起来就没有页面可退出。这一页留在屏上,
        // 状态落到 Wi-Fi 重试档 —— 长按确定是重开热点,确定是用存量凭据重试。
        s_state = ST_WIFI_RETRY;
        show_prov_screen("热点启动失败", why, PROV_AP_HINT, "", "");
        return;
    }
    s_prov_ap = true;
    show_prov_screen("手机连接热点,浏览器打开", s_prov_a, "无需密码",
                     "http://192.168.4.1", PROV_AP_HINT);
    s_state = ST_PROV;
}

// 起局域网配置服务并把配对码打在屏幕上,正文见 open_lan_prov。
static void open_lan_prov(void);
// 会话结束后的收尾:它决定回哪一页,配对码页的长按出口也要走同一条路。
static void handle_prov_done(void);

// 从 BOOT 走一遍:载入配置 → 有 Wi-Fi 就连,没有就起引导热点。
static void enter_boot(void)
{
    (void)app_cfg_load();
    s_snapshot_wall = 0;   // 重配回来是一份新开局:旧的同步时刻不再作数
    if (!app_cfg_has_wifi()) {
        start_ap_prov();
        return;
    }
    if (app_wifi_init() != ESP_OK || !wifi_connect_and_run()) {
        if (!s_err[0] && !s_note[0]) set_err("Wi-Fi 初始化失败");
        s_state = ST_WIFI_RETRY;
        reenter_list(); // 列表页兼作重试页,不能停在配网最后一帧
        return;
    }
    np_poll_init(&s_poll, app_cfg_poll_base_s(), app_cfg_poll_cap_s());
    s_auth_halted = false;
    s_state = ST_RUN;
    if (!app_cfg_has_tailscale()) {
        set_note("待配置 Tailscale 凭据");
        // 没凭据就没有设备行,列表页什么也显示不了。直接落到配对码页面,
        // 而不是让用户自己找"网络配置"。
        if (!s_prompt_dismissed && app_wifi_has_ip()) {
            open_lan_prov();
            if (s_state == ST_PROV) return; // 服务真起来了才交给配网页
        }
    } else {
        clear_status();
    }
    reenter_list();
}

static void open_lan_prov(void)
{
    // 没地址就没什么可配置的:mDNS 广播和 httpd 都绑在这个 IPv4 上,
    // 提前起服务只会把 http://0.0.0.0 印到屏幕上(np-0.1.10 实测症状)。
    if (!app_wifi_has_ip()) {
        ESP_LOGW(TAG, "未取得 IP,暂不启动局域网配置服务");
        if (bsp_lvgl_lock(500)) {
            set_note("尚未取得 IP,请稍后再试");
            bsp_lvgl_unlock();
        }
        return;
    }
    // 上一轮会话没收尾就先停:app_prov_start 见会话非空必然挡回 INVALID_STATE。
    app_prov_stop();
    const esp_err_t err = app_prov_start(NP_PROV_LAN, NULL, 0,
                                         s_prov_b, sizeof(s_prov_b));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "局域网配置服务启动失败: %s", esp_err_to_name(err));
        if (!bsp_lvgl_lock(500)) return;
        set_err("配置服务启动失败 %s", esp_err_to_name(err));
        bsp_lvgl_unlock();
        return;
    }
    // 很多系统解析不了 mDNS 的 .local,补一行直连 IP 兜底。
    s_prov_ap = false;
    char ip[16];
    app_wifi_ip_str(ip, sizeof(ip));
    strlcpy(s_prov_ip, ip, sizeof(s_prov_ip));
    np_prompt_lan(&s_prov_lines, app_cfg_ssid(), ip, s_prov_b);
    show_prov_screen(s_prov_lines.line[0], s_prov_lines.line[1],
                     s_prov_lines.line[2], s_prov_lines.line[3],
                     PROV_EXIT_HINT);
    s_state = ST_PROV;
}

// 配对码页上的地址是打开那一刻的快照,而断链自愈会在用户读网页期间换租约:
// 旧地址成了死链,页面怎么点都打不开(np-0.1.18 实测,重进配网页才好)。
// 每秒复查一次,地址变了就重拼四行;配对码、会话、热点都不动。
static void refresh_prov_addr(void)
{
    char ip[16];
    app_wifi_ip_str(ip, sizeof(ip));
    // 四行里任意一行变了才重画:多半是 DHCP 换了地址,少数是换了一个 AP。
    if (!np_prompt_lan_update(&s_prov_lines, app_cfg_ssid(), ip, s_prov_b)) return;
    ESP_LOGI(TAG, "配置页重画:%s → %s",
             s_prov_ip[0] ? s_prov_ip : "(无)", ip[0] ? ip : "(无)");
    strlcpy(s_prov_ip, ip, sizeof(s_prov_ip));
    show_prov_screen(s_prov_lines.line[0], s_prov_lines.line[1],
                     s_prov_lines.line[2], s_prov_lines.line[3],
                     PROV_EXIT_HINT);
}

// ---------------------------------------------------------------- 事件处理
// 状态页正文五行。列表为空时"确定"也落到这里:没凭据就没有设备行,而
// "网络配置"只挂在状态页上,不给入口便无法补填凭据。
// 最后两行是账号与登录状态:账号取配置里那个尾网名(Tailscale 的尾网名就是
// 登录用的账号名),登录状态取本次开机里真实发生过的结果,不猜服务端记账号。
static void show_status_view(void)
{
    char l1[96], l2[96], l3[96], l4[96], l5[96];
    np_account_text(app_cfg_tailnet(), l4, sizeof(l4));
    np_login_text(np_login_classify(app_cfg_has_tailscale(), s_auth_halted,
                                    s_last_sync > 0),
                  l5, sizeof(l5));
    if (s_err[0] != '\0') {
        // 出错时先讲哪一层不通、数字细节、时钟状态,配置参数让位(串口日志里全有)。
        snprintf(l1, sizeof(l1), "异常:%s", s_err);
        if (s_err_detail[0] != '\0') snprintf(l2, sizeof(l2), "细节:%s", s_err_detail);
        else l2[0] = '\0';
        snprintf(l3, sizeof(l3), "时钟:%s 退避:%lu 秒",
                 clock_synced() ? "已校" : "未校",
                 (unsigned long)s_poll.current_delay_s);
    } else if (s_note[0] != '\0') {
        // 等待中的状态页:说清在等什么、时钟走到哪一步,但不写"异常"。
        snprintf(l1, sizeof(l1), "正在等待:%s", s_note);
        snprintf(l2, sizeof(l2), "时钟:%s 退避:%lu 秒",
                 clock_synced() ? "已校" : "未校",
                 (unsigned long)s_poll.current_delay_s);
        snprintf(l3, sizeof(l3), "轮询:%lu 秒(上限 %lu)",
                 (unsigned long)app_cfg_poll_base_s(),
                 (unsigned long)app_cfg_poll_cap_s());
    } else {
        snprintf(l1, sizeof(l1), "上次同步:%s",
                 s_last_sync > 0 ? "成功" : (s_last_sync < 0 ? "失败" : "还没试过"));
        snprintf(l2, sizeof(l2), "轮询:%lu 秒(上限 %lu)",
                 (unsigned long)app_cfg_poll_base_s(),
                 (unsigned long)app_cfg_poll_cap_s());
        snprintf(l3, sizeof(l3), "退避中:%lu 秒",
                 (unsigned long)s_poll.current_delay_s);
    }
    if (!bsp_lvgl_lock(500)) return;
    np_ui_show_status(l1, l2, l3, l4, l5);
    bsp_lvgl_unlock();
}

// 屏幕不再自动降亮度(用户要求):热点页要靠屏幕读热点名,凭据录入页要靠
// 屏幕读配对码,熄屏还会让人误判设备已经掉线。按键处理里因此不需要恢复亮度。
static void handle_key(const app_ev_t *key)
{
    const bsp_btn_t btn = key->btn;
    const bsp_btn_ev_t ev = key->ev;

    // 熄屏唤醒:任意键先恢复背光,再继续正常分发。黑屏只是背光关了,原视图还在;
    // AOD 页则由 np_ui_handle_key 自己切回列表。每次按键都刷新最后活动时刻。
    if (s_screen_off) {
        bsp_display_backlight(app_cfg_brightness());
        s_screen_off = false;
        s_aod_active = false;
    }
    s_last_activity_ms = esp_timer_get_time() / 1000;

    if (s_state == ST_PROV) {
        if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
            // 退出这一页只做两件事:关掉会话,然后按链路真相决定去哪一页。
            // 不从这里重走整套开机阻塞连接 —— 那正是把按键冻住二十几秒的那条路。
            app_prov_stop();
            s_prompt_dismissed = true; // 长按就是"这轮不填",别再自动弹回去
            handle_prov_done();
        }
        return;
    }
    if (s_state == ST_WIFI_RETRY) {
        if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
            // 密码填错时不必修硬件:回引导热点重填。
            start_ap_prov();
            return;
        }
        if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK) {
            if (!app_cfg_has_wifi()) { enter_boot(); return; }
            if (app_wifi_init() == ESP_OK && wifi_connect_and_run()) {
                np_poll_init(&s_poll, app_cfg_poll_base_s(), app_cfg_poll_cap_s());
                s_state = ST_RUN;
                clear_status();
            }
            render_now();
        }
        return;
    }

    // 按键处理会在锁内改选中态与提示行,必须在锁里执行;而会话的开关(下面
    // 局域网/热点两项)不能握着一把 1 秒级的锁做,所以这一层只管把导航意图
    // 取出来就撒手。
    np_key_result_t r;
    if (!bsp_lvgl_lock(500)) {
        // 锁被整轮刷屏占住不等于这颗键没按过:原样再发一次,迟一拍再试。
        // 发满两次还拿不到才丢,并把它写进日志 —— 一句日志不留正是"按了没反应"。
        app_ev_t again = *key;
        again.retry++;
        if (key->retry < 2u && post_ev(&again)) {
            ESP_LOGW(TAG, "渲染锁未空,按键再发第 %u 次", (unsigned)again.retry);
            return;
        }
        ESP_LOGW(TAG, "渲染锁未空,丢弃按键 btn=%d ev=%d", (int)key->btn,
                 (int)key->ev);
        return;
    }
    r = np_ui_handle_key(btn, ev);
    bsp_lvgl_unlock();
    switch (r) {
    case NP_KEY_OPEN_DETAIL:
        // 列表为空时"确定"落到状态页:没凭据就没有设备行,而菜单只挂在这一页上。
        if (!open_detail(np_ui_selected())) show_status_view();
        break;
    case NP_KEY_OPEN_STATUS:
        show_status_view();
        break;
    case NP_KEY_OPEN_SETTINGS:
        if (!bsp_lvgl_lock(500)) return;
        np_ui_settings_enter();
        bsp_lvgl_unlock();
        break;
    case NP_KEY_BACK_LIST:
        render_now();
        break;
    case NP_KEY_ACTION_SYNC:
        s_poll.next_run_s = 0;
        s_auth_halted = false;
        break;
    case NP_KEY_ACTION_PROV:
        open_lan_prov();
        break;
    case NP_KEY_ACTION_AP:
        start_ap_prov();
        break;
    case NP_KEY_ACTION_CLEAR: {
        const int64_t now_ms = esp_timer_get_time() / 1000;
        if (now_ms - s_clear_prompt_ms > 5000) {
            s_clear_prompt_ms = now_ms;
            // 只换提示行:整页重建会把选中项一起抹掉,第二拍就落到 0 号"立即
            // 刷新"上(np-0.1.22 实测:清除凭据怎么按都刷不掉了)。
            if (!bsp_lvgl_lock(500)) return;
            np_ui_status_hint("再按一次确定 = 清除凭据");
            bsp_lvgl_unlock();
            return;
        }
        s_clear_prompt_ms = 0;
        app_cfg_clear_tailscale();
        (void)app_cfg_save();
        s_auth_halted = false;
        s_last_sync = 0;   // 凭据没了,登录状态回到"未配置"
        s_poll.next_run_s = 0;
        // 清完就等于回到"没凭据",开机时那条自动弹配对页的路径不能再被上一轮的
        // "用户手动关掉"抑制着。
        s_prompt_dismissed = false;
        // 只留一句"可在配置网页重新填写"等于死链:没有地址和配对码,网页根本进不去
        // (用户报:清除后屏幕没给重新输入的 IP/网址)。走开机时那条现成的路。
        if (app_wifi_has_ip()) {
            open_lan_prov();   // 自带失败文案与会话关闭,不能握着 LVGL 锁调用
            if (s_state == ST_PROV) break;
            if (!bsp_lvgl_lock(500)) return;
            np_ui_show_status("凭据已清除",
                              s_err[0] ? s_err : "配置服务没起来,长按返回再试",
                              "", "", "");
            bsp_lvgl_unlock();
            break;
        }
        if (!bsp_lvgl_lock(500)) return;
        np_ui_show_status("凭据已清除", "Wi-Fi 未联网,先到列表页重新配网", "", "", "");
        bsp_lvgl_unlock();
        break;
    }
    default:
        break;
    }
}

static void handle_prov_done(void)
{
    const bool wifi_changed = app_prov_wifi_applied();
    const bool ts_applied = app_prov_ts_applied();
    if (s_prov_ap || wifi_changed) {
        s_prov_ap = false;
        app_wifi_stop();
        enter_boot(); // 由它重建列表屏
        return;
    }
    if (ts_applied) {
        np_poll_init(&s_poll, app_cfg_poll_base_s(), app_cfg_poll_cap_s());
        s_auth_halted = false;
        np_clock_init(&s_clock);
        s_last_sync = 0;   // 换了一套凭据,上一次的成功不能再算作"已登录"
        // 首次登录不该带着 1970 年的钟出门:没校上就重新起一轮校时,请求由 poll
        // 任务的门禁扣住,校上了才发。屏幕上先给一句"正在登录"。
        if (!clock_synced()) clock_rearm();
        // 这句要一直挂到首次同步有结果为止:真在等什么(等链路/等校时)的时候,
        // 那几条更具体的文案会自己顶掉它,而"就绪那一拍撤下等待"不会在请求还在
        // 路上时把它先清掉,变成一闪而过。
        set_note("凭据已保存,正在登录");
        s_poll.next_run_s = 0;
    } else if (!app_cfg_has_tailscale()) {
        s_prompt_dismissed = true; // 提示页挂到超时也没人填,不再自动重开
    }
    // 这一页结束的时候链路是什么状态就写什么:配置页里那一次扫描很可能已经把
    // STA 扫断了。以前这里无条件写 ST_RUN,于是列表页页脚说"已断"而轮询又被
    // 状态那一栏放行、按键又被那条 30 秒阻塞重连冻住 —— 三件事各说各话。
    s_state = app_wifi_has_ip() ? ST_RUN : ST_WIFI_RETRY;
    reenter_list();
}

// ---------------------------------------------------------------- 内存遥测
// 采集放在应用层而不是 np_mem.c:lv_mem_monitor() 要遍历 LVGL 池的管理结构,
// 只能在持 LVGL 锁时做,而纯模块不该依赖 IDF 与 LVGL(它要能单机跑测试)。
// 池子容量由编译期钉住:sdkconfig 里那个已废弃的 KB 键骗过我们一次(np-0.1.26
// 查清:配置写 24、镜像实际吃 64 KB),以后改 CONFIG_LV_MEM_SIZE 不同步
// np_mem.h 的预期值就直接编不过。
ESP_STATIC_ASSERT(CONFIG_LV_MEM_SIZE == NP_LV_POOL_BYTES,
                  "LVGL pool differs from NP_LV_POOL_BYTES; change both together");

// poll 循环本身 500 ms 一拍,遥测按这个倍数采样(20 秒一次):再密会在 LVGL 锁
// 上多占一圈,更疏则抓不到失败前的稳态。
#define MEM_LOG_TICKS 40u

static np_mem_snap_t s_mem;

void np_mem_capture(np_mem_snap_t *s)
{
    if (s == NULL) return;
    s->free_heap = (int)esp_get_free_heap_size();
    s->largest_blk = (int)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    s->dma_free = (int)heap_caps_get_free_size(MALLOC_CAP_DMA);
    s->dma_blk = (int)heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    // 拿不到锁就留 lv_total=0:np_mem_format() 会整段省略,而不是打一串 0
    // 冒充实测值。200 ms 是有意的短等 —— 遥测不该拖慢轮询循环。
    s->lv_total = 0u;
    if (bsp_lvgl_lock(200)) {
        lv_mem_monitor_t mon;
        lv_mem_monitor(&mon);
        bsp_lvgl_unlock();
        s->lv_total = (unsigned)mon.total_size;
        s->lv_free = (unsigned)mon.free_size;
        s->lv_biggest = (unsigned)mon.free_biggest_size;
        s->lv_max_used = (unsigned)mon.max_used;
        s->lv_used_pct = (unsigned)mon.used_pct;
        s->lv_frag_pct = (unsigned)mon.frag_pct;
    }
}

void np_mem_last(np_mem_snap_t *s)
{
    if (s == NULL) return;
    *s = s_mem;   // 最近一次采样,最坏滞后一个采样周期
}

// ---------------------------------------------------------------- 任务
static void poll_task(void *arg)
{
    (void)arg;
    // 初值让开机第一拍就采一次:那一拍的堆数字是后面所有比较的基准。
    static uint32_t mem_ticks = MEM_LOG_TICKS;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(500));
        const int64_t now_ms = esp_timer_get_time() / 1000;
        if (++mem_ticks >= MEM_LOG_TICKS) {
            mem_ticks = 0u;
            np_mem_capture(&s_mem);
            char memline[NP_MEM_LINE_CAP];
            np_mem_format(&s_mem, memline, sizeof(memline));
            ESP_LOGI(TAG, "mem %s", memline);
        }
        // 断链自愈单独一拍:配置页的扫描会把射频借走,驱动在那次断开事件里的重连
        // 请求可能被吞掉,链路就停在断开态不再有人管(与当前停在哪个状态无关)。
        // 这一拍是断链期间唯一的周期重连来源 —— 按键任务不参与射频。
        // 局域网会话开着时不让它整栈重建: httpd 与 mDNS 就绑在这块 netif 上。
        app_wifi_reconcile(now_ms, app_prov_mode() != NP_PROV_LAN);

        const bool up = app_wifi_has_ip();  // 没地址时 HTTPS 必然失败,别烧退避步数
        const bool synced = clock_synced();

        // 详情扩展信息排在整表之前:用户刚按下确定,别让他等下一轮轮询的节奏。
        // 门禁与整表同一件事:没地址连 DNS 都解不了,钟没校上必然撞在"有效期还
        // 没开始"的证书上(np-0.1.22 实机第二次反馈就是那句"证书有效期不对")。请求
        // 继续挂着,链路和时钟到位自然会走到,只是要把"获取中"改成实话,否则用户
        // 盯着一行 "-" 不知道在等什么。
        char want_id[NP_ID_LEN];
        uint32_t want_gen = 0;
        if (peek_detail_request(want_id, sizeof(want_id), &want_gen)) {
            if (!up || !synced) {
                if (bsp_lvgl_lock(1000)) {
                    const char *wait_note = up ? "扩展信息在等校时"
                                               : "扩展信息在等网络";
                    if (strcmp(s_detail_note, wait_note) != 0) {
                        strlcpy(s_detail_note, wait_note, sizeof(s_detail_note));
                        if (np_ui_view() == NP_VIEW_DETAIL) paint_detail();
                    }
                    bsp_lvgl_unlock();
                }
            } else {
                np_detail_t fresh;
                const ts_result_t dr = ts_api_fetch_device(want_id, &fresh);
                if (bsp_lvgl_lock(1000)) {
                    // 世代对得上才收货。期间换过机器就整份作废、请求也不撤销,
                    // 下一拍会按新机器重新拉一次。
                    if (want_gen == s_detail_gen) {
                        s_detail_req = false;
                        if (dr == TS_OK) {
                            s_detail = fresh;
                            s_detail_ok = true;
                            s_detail_note[0] = '\0';
                        } else {
                            np_err_src_t src;
                            err_src(&src);
                            char cause[24];
                            np_errfmt_cause(cause, sizeof(cause), &src);
                            s_detail_ok = false;
                            snprintf(s_detail_note, sizeof(s_detail_note),
                                     "扩展信息未取得:%s", cause);
                        }
                        if (np_ui_view() == NP_VIEW_DETAIL) paint_detail();
                    }
                    bsp_lvgl_unlock();
                }
            }
        }

        const int64_t now = (int64_t)time(NULL);
        // 整表的请求只在钟真校上之后才发(等待与重起的节奏见 np_clock_tick)。
        // 退避阶梯在等待期间冻住:没发出去的请求不算失败,别把"还在等"走成
        // "同步失败"那一串加倍间隔。
        const np_clock_action_t gate = np_clock_tick(&s_clock, up, synced, now_ms);
        np_poll_set_net(&s_poll, up && synced);
        if (gate != NP_CLOCK_GO) {
            if (gate == NP_CLOCK_REARM) {
                clock_rearm();
                ESP_LOGW(TAG, "校时未回,第 %u 轮重起 SNTP",
                         (unsigned)np_clock_rearms(&s_clock));
            }
            if (gate == NP_CLOCK_IDLE) {
                // 开机后还没拿到过地址时正在连接流程里,别说"已断"。
                if (s_link_seen) set_wait_note(NOTE_LINK_DOWN);
            } else {
                set_wait_note(np_clock_stalled(&s_clock) ? NOTE_CLOCK_STALL
                                                         : NOTE_CLOCK_WAIT);
            }
            continue;
        }
        set_wait_note(NOTE_NONE);
        if (s_auth_halted || !app_cfg_has_tailscale()) continue;
        if (!np_poll_due(&s_poll, now)) continue;

        const ts_result_t r = ts_api_fetch_devices(np_lists_back());
        if (r == TS_OK) {
            s_last_sync = 1;   // 服务端认了这套凭据,登录状态这才算"已登录"
            np_poll_on_success(&s_poll, now);
            clear_status();
            if (bsp_lvgl_lock(2000)) {
                np_lists_swap();
                // 记下这一刻,不在渲染时猜:渲染那一刻手上只有"现在",
                // 而"这份数据是什么时候的"只有换它的人知道。
                s_snapshot_ms = esp_timer_get_time() / 1000;
                s_snapshot_wall = (int64_t)time(NULL);
                bsp_lvgl_unlock();
                (void)post(EV_RENDER, BSP_BTN_UP, BSP_BTN_CLICK);
            }
        } else {
            np_err_src_t src;
            err_src(&src);
            np_poll_on_fail(&s_poll, now);
            s_last_sync = -1;
            if (r == TS_ERR_AUTH) s_auth_halted = true;  // 凭据被拒不再重试
            np_errfmt_cause(s_err, sizeof(s_err), &src);
            np_errfmt_detail(s_err_detail, sizeof(s_err_detail), &src);
            s_note[0] = '\0';   // 真失败顶掉等待行
            ESP_LOGW(TAG, "同步失败 result=%d 原因=%s 细节=%s", (int)r, s_err,
                     s_err_detail);
            (void)post(EV_RENDER, BSP_BTN_UP, BSP_BTN_CLICK);
        }
    }
}

// 进入熄屏:AOD 开则切到时间+在线数页(低亮度常显),否则直接关背光。
// 只在 ST_RUN 调(配网页/热点页要一直亮着读信息)。
static void enter_screen_off(void)
{
    if (app_cfg_aod_enabled()) {
        if (bsp_lvgl_lock(500)) {
            char hm[8];
            np_time_format_hm(hm, sizeof(hm), (int64_t)time(NULL),
                              app_cfg_tz_offset_s());
            const np_list_t *snap = np_lists_active();
            const size_t online = np_list_online_count(snap);
            np_ui_aod_show(hm, online, snap->total, s_last_sync > 0);
            bsp_lvgl_unlock();
        }
        bsp_display_backlight(20);
        s_aod_active = true;
    } else {
        bsp_display_backlight(0);
    }
    s_screen_off = true;
}

static void app_task(void *arg)
{
    (void)arg;
    // 按键由 main.c 初始化并转发(唤醒守卫在那里),本任务只消费队列。
    // 列表屏是兜底载体(连 Wi-Fi 重试时也要有画面)。
    if (bsp_lvgl_lock(1000)) {
        np_ui_list_enter();
        bsp_lvgl_unlock();
    }
    // 首采要赶在开机流程之前:开机失败落重试页的首次渲染就用真值,不然
    // s_battery 还是 -1,电池图标会以空壳样式冻在开机第一帧。
    s_battery = bsp_battery_soc();
    enter_boot();
    s_last_activity_ms = esp_timer_get_time() / 1000;

    int64_t last_render_ms = esp_timer_get_time() / 1000;
    int battery_counter = 0;
    for (;;) {
        app_ev_t e;
        const int64_t now_ms = esp_timer_get_time() / 1000;
        int64_t wait = 1000 - (now_ms - last_render_ms);
        if (wait < 20) wait = 20;
        if (xQueueReceive(s_queue, &e, pdMS_TO_TICKS(wait)) == pdTRUE) {
            switch (e.type) {
            case EV_KEY: {
                // 这一行量的是"按键到处理"这一段等了多久:时间全花
                // 在队列里等这一拍(我们的锁)、还是花在采样任务没被调度上(组件
                // 那颗 5ms 的 esp_timer 心跳,priority 1)。前者是我们的账,后者
                // 要动全局优先级,不属于这一轮。
                const int32_t age = (int32_t)(now_ms32() - e.posted_ms);
                if (age > 300) {
                    ESP_LOGW(TAG, "按键延迟 %ldms retry=%u", (long)age,
                             (unsigned)e.retry);
                }
                handle_key(&e);
                break;
            }
            case EV_RENDER: render_now(); last_render_ms = now_ms; break;
            }
        }
        const int64_t t2 = esp_timer_get_time() / 1000;
        if (t2 - last_render_ms >= 1000) {
            last_render_ms = t2;
            s_time_synced = clock_synced();
            if (s_state == ST_WIFI_RETRY && app_wifi_has_ip()) {
                // 断链期间的周期重连只有那条 500ms 阶梯一个来源(它在 poll 任务
                // 里跑,与停在哪个状态无关)。这一拍只做"链路回来了就把状态收回去",
                // 不再自己上手拆链路:以前每 30 秒从这里走一遍 20+8 秒的阻塞连接,
                // 而那二十几秒冻住的正是队列唯一的消费者 —— 按键。
                np_poll_init(&s_poll, app_cfg_poll_base_s(),
                             app_cfg_poll_cap_s());
                np_clock_init(&s_clock);
                sntp_start();
                s_state = ST_RUN;
                clear_status();
            }
            // 会话收尾:以前由 prov_wd 那个任务自己拆自己通知,现在它只报一句
            // "该关了",拆与后续都在这一拍里做 —— 全程只有一个任务开关会话。
            if (s_state == ST_PROV && app_prov_take_close(t2 * 1000)) {
                app_prov_stop();
                handle_prov_done();
            }
            if (s_state == ST_RUN) render_now();
            // 自动熄屏:只在正常运行态检查(配网页/热点页要一直亮着读信息)。
            if (s_state == ST_RUN && !s_screen_off &&
                np_display_cfg_should_sleep(t2, s_last_activity_ms,
                                            app_cfg_off_minutes())) {
                enter_screen_off();
            }
            // 熄屏显示开启时,每秒刷新 AOD 页的时间与在线数。
            if (s_aod_active && bsp_lvgl_lock(500)) {
                char hm[8];
                np_time_format_hm(hm, sizeof(hm), (int64_t)time(NULL),
                                  app_cfg_tz_offset_s());
                const np_list_t *snap = np_lists_active();
                const size_t online = np_list_online_count(snap);
                np_ui_aod_update(hm, online, snap->total, s_last_sync > 0);
                bsp_lvgl_unlock();
            }
            // 配对码页停在屏幕上时列表/详情都不再重绘,页上的地址只能专门复查。
            if (s_state == ST_PROV && !s_prov_ap) refresh_prov_addr();
            if (app_prov_take_sync_request()) s_poll.next_run_s = 0;
            // "再按一次"过期后那句话就不作数了:留在屏幕上会让人以为这一拍还是清除。
            if (s_clear_prompt_ms &&
                t2 - s_clear_prompt_ms > 5000) {
                s_clear_prompt_ms = 0;
                if (bsp_lvgl_lock(500)) {
                    np_ui_status_hint(NULL);
                    bsp_lvgl_unlock();
                }
            }
        }
        if (++battery_counter >= 10) {
            battery_counter = 0;
            const int soc = bsp_battery_soc();
            if (soc != s_battery) {
                s_battery = soc;
                // 采样路径不产生重绘,出错态(重试页等)又没有 1 秒周期重绘,
                // 值变了必须自己发一次渲染事件,图标才跟得上真实电量。
                (void)post(EV_RENDER, BSP_BTN_UP, BSP_BTN_CLICK);
            }
        }
    }
}

esp_err_t np_app_start(void)
{
    np_lists_init();
    s_queue = xQueueCreate(16, sizeof(app_ev_t));
    if (!s_queue) return ESP_ERR_NO_MEM;
    if (xTaskCreate(app_task, "np_app", 8192, NULL, 6, NULL) != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(poll_task, "np_poll", 8192, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "轮询任务创建失败");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
