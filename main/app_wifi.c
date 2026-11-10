// main/app_wifi.c —— 只做 STA:连接/重连/状态查询。
// SoftAP 引导模式由 app_prov 负责让出射频(先调 app_wifi_stop)。
#include "app_wifi.h"

#include "app_cfg.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/ip4_addr.h"

#include "np_link.h"
#include "np_scanlist.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "app_wifi";

#define CONNECTED_BIT BIT0
#define FAIL_BIT      BIT1
#define IP_OK_BIT     BIT2
// 认证类失败要在里面重试(见 AUTH_TRY_MAX),窗口得装得下整轮重试:
// 4-way 超时单次就要几秒,12 秒只够试两次,第三次还没回来就被判成"连不上"。
#define CONNECT_TIMEOUT_MS 20000
#define AUTH_TRY_MAX 3
// 关联成功 != 能上网:mDNS/局域网配置服务都要靠这个地址,拿不到就别起服务。
#define IP_TIMEOUT_MS 8000

static esp_netif_t *s_netif;
static esp_event_handler_instance_t s_connected_h;
static esp_event_handler_instance_t s_disconnected_h;
static esp_event_handler_instance_t s_got_ip_h;
static EventGroupHandle_t s_events;
static bool s_started;
static volatile bool s_up;
static volatile bool s_has_ip;
static volatile int8_t s_rssi;
static volatile uint8_t s_auth_try;
static np_link_t s_link;          // 断链补发阶梯(见 np_link.h)
static volatile bool s_manual;    // 上层正在走阻塞连接,阶梯别插手
static volatile bool s_scan_busy; // 扫描占用射频期间,阶梯也别插手(见 app_wifi_reconcile)
static volatile int64_t s_no_ip_ms; // 关联成功但还没拿到地址的起点
// 漫游选网频率上限(见 kick_with_roam):上次选网扫描的时刻,只由 poll 任务
// 经 reconcile 触碰。轮询每 30 秒最多借一次射频去选网,免得断链期间扫描与
// 驱动的连接尝试互相踩踏。
static int64_t s_roam_ms;
#define ROAM_INTERVAL_MS 30000

static void on_connected(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id; (void)data;
    s_up = true;
    s_auth_try = 0;
    s_no_ip_ms = esp_timer_get_time() / 1000;
    np_link_up(&s_link);
    xEventGroupSetBits(s_events, CONNECTED_BIT);
}

static void on_got_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
    // 只认自己这块 STA netif,且必须还在关联状态:引导热点顶不上来,
    // 换网络时上一条链路迟到的地址事件也不能算数。
    if (ev == NULL || ev->esp_netif != s_netif || !s_up) return;
    s_has_ip = ev->ip_info.ip.addr != 0u;
    if (s_has_ip) {
        xEventGroupSetBits(s_events, IP_OK_BIT);
        ESP_LOGI(TAG, "取得 IP " IPSTR, IP2STR(&ev->ip_info.ip));
    }
}

static bool is_auth_reason(uint8_t reason)
{
    return reason == WIFI_REASON_AUTH_FAIL ||
           reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
           reason == WIFI_REASON_HANDSHAKE_TIMEOUT ||
           reason == WIFI_REASON_MIC_FAILURE;
}

static void on_disconnected(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id;
    wifi_event_sta_disconnected_t *ev = (wifi_event_sta_disconnected_t *)data;
    const uint8_t reason = ev ? ev->reason : 0;
    s_up = false;
    s_has_ip = false;
    s_no_ip_ms = 0;
    xEventGroupClearBits(s_events, IP_OK_BIT);
    s_rssi = 0;
    np_link_down(&s_link, esp_timer_get_time() / 1000);
    ESP_LOGW(TAG, "Wi-Fi 断开 reason=%u", (unsigned)reason);
    if (is_auth_reason(reason)) {
        // 首次上电的头一次关联偶尔会倒在 4-way 超时上,这时报"密码错误"是冤枉
        // 人:先自己重连,连着三次都死在认证上才认这个结论。真密码错也就多等
        // 两三秒(AUTH_FAIL 回来得快),不会把用户卡在列表页。
        if (s_started && s_auth_try + 1u < (uint8_t)AUTH_TRY_MAX) {
            s_auth_try++;
            if (esp_wifi_connect() == ESP_OK) {
                ESP_LOGW(TAG, "认证类失败 reason=%u,自动重试第 %u 次",
                         (unsigned)reason, (unsigned)s_auth_try);
                return;
            }
            // 连接请求没被受理,跟密码没关系,交给超时那条路说实话。
            return;
        }
        // 试满了才不再重连:置 FAIL 让上层把错误显示出来。
        xEventGroupSetBits(s_events, FAIL_BIT);
        return;
    }
    // 其余原因一律不在这里补发连接,交给阶梯:驱动在断开时自带一轮内部重试,
    // 对"已在别处"的网那一轮注定完不成;我们再补 esp_wifi_connect 只会把它整
    // 整重启一遍 —— 驱动停在"对死网的连接循环"上期间,上层每一个选网扫描都被
    // 打回 ESP_ERR_WIFI_STATE,多网络设备永远换不到另一张网(实测结论)。阶梯
    // 的补发自带"选网前先让射频空闲"的重建路径,3 秒一档够快,不缺这一拍。
}

// 用 app_cfg 里的凭据组一份 wifi_config(非阻塞路径与阻塞路径共用一份定义)。
static void fill_config(wifi_config_t *wc)
{
    memset(wc, 0, sizeof(*wc));
    strlcpy((char *)wc->sta.ssid, app_cfg_ssid(), sizeof(wc->sta.ssid));
    strlcpy((char *)wc->sta.password, app_cfg_psk(), sizeof(wc->sta.password));
    wc->sta.threshold.authmode = wc->sta.password[0] ? WIFI_AUTH_WPA2_PSK
                                                     : WIFI_AUTH_OPEN;
}

// 只发起连接、不等事件:断链自愈走这条,免得 500ms 节拍被 20 秒阻塞吃掉。
static void kick_connect(void)
{
    wifi_config_t wc;
    fill_config(&wc);
    const esp_err_t scerr = esp_wifi_set_config(WIFI_IF_STA, &wc);
    if (scerr != ESP_OK) {
        ESP_LOGW(TAG, "写入连接配置失败 %s", esp_err_to_name(scerr));
        return;
    }
    const esp_err_t cerr = esp_wifi_connect();
    if (cerr != ESP_OK) ESP_LOGW(TAG, "补发连接未受理:%s", esp_err_to_name(cerr));
}

esp_err_t app_wifi_init(void)
{
    if (s_started) return ESP_OK;
    np_link_init(&s_link);

    if (!s_events) {
        s_events = xEventGroupCreate();
        if (!s_events) return ESP_ERR_NO_MEM;
    }

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_WIFI_STA();
    s_netif = esp_netif_new(&netif_cfg);
    if (!s_netif) return ESP_ERR_NO_MEM;

    esp_err_t err = esp_netif_attach_wifi_station(s_netif);
    if (err != ESP_OK) goto fail;
    err = esp_wifi_set_default_wifi_sta_handlers();
    if (err != ESP_OK) goto fail;

    err = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED,
                                              on_connected, NULL, &s_connected_h);
    if (err != ESP_OK) goto fail;
    err = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                              on_disconnected, NULL, &s_disconnected_h);
    if (err != ESP_OK) goto fail;
    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              on_got_ip, NULL, &s_got_ip_h);
    if (err != ESP_OK) goto fail;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) goto fail;

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) goto fail_deinit;
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) goto fail_deinit;
    err = esp_wifi_start();
    if (err != ESP_OK) goto fail_deinit;
    s_started = true;
    return ESP_OK;

fail_deinit:
    esp_wifi_deinit();
fail:
    if (s_connected_h) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED,
                                              s_connected_h);
        s_connected_h = NULL;
    }
    if (s_disconnected_h) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                              s_disconnected_h);
        s_disconnected_h = NULL;
    }
    if (s_got_ip_h) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_got_ip_h);
        s_got_ip_h = NULL;
    }
    if (s_netif) {
        esp_netif_destroy(s_netif);
        s_netif = NULL;
    }
    return err;
}

// 手上这条链路是不是就是配置要的那条:是的话重连一步都不用做。
// 少问这一句的后果是每次"重连"都先无条件拆掉好链路,再从头等关联与地址,
// 而那二十几秒冻住的正是按键任务本身(np-0.1.27 报的"进配置页之后按钮不动")。
static bool link_matches_config(void)
{
    if (!s_up || !s_has_ip) return false;
    wifi_ap_record_t rec;
    if (esp_wifi_sta_get_ap_info(&rec) != ESP_OK) return false;
    const char *want = app_cfg_ssid();
    const size_t want_len = strlen(want);
    if (want_len >= sizeof(rec.ssid)) return false;
    // 多比一个字节:两侧的名字要在同一个位置上都是结尾,长名字截断才算不一致。
    return strncmp((const char *)rec.ssid, want, want_len + 1u) == 0;
}

static esp_err_t connect_blocking(void)
{
    if (!s_started) return ESP_ERR_INVALID_STATE;
    if (!app_cfg_has_wifi()) return ESP_ERR_INVALID_STATE;

    if (link_matches_config()) {
        ESP_LOGI(TAG, "链路已就绪且与配置一致,不重连");
        return ESP_OK;
    }
    // 先拆一次:卡在"正在连接"时 esp_wifi_connect 会被原地吞掉(见 np_link.h)。
    // 但没链路可拆的时候不必白等那 100 毫秒,那只是把按键多冻十分之一秒。
    (void)esp_wifi_disconnect();
    if (s_up) vTaskDelay(pdMS_TO_TICKS(100));
    xEventGroupClearBits(s_events, CONNECTED_BIT | FAIL_BIT | IP_OK_BIT);
    s_has_ip = false;
    s_auth_try = 0;

    wifi_config_t wc;
    fill_config(&wc);

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wc);
    if (err != ESP_OK) return err;
    err = esp_wifi_connect();
    if (err != ESP_OK) return err;

    EventBits_t bits = xEventGroupWaitBits(s_events, CONNECTED_BIT | FAIL_BIT,
                                           pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(CONNECT_TIMEOUT_MS));
    if (!(bits & CONNECTED_BIT)) {
        return (bits & FAIL_BIT) ? APP_ERR_WIFI_AUTH_FAIL : ESP_ERR_TIMEOUT;
    }
    ESP_LOGI(TAG, "已关联 %s,等待地址", app_cfg_ssid());

    // 关联之后还要等 DHCP:配置服务与 mDNS 都绑在这个地址上,提前返回
    // 会把屏幕写成 http://0.0.0.0,用户根本无从判断连没连上。
    bits = xEventGroupWaitBits(s_events, IP_OK_BIT | FAIL_BIT, pdTRUE, pdFALSE,
                               pdMS_TO_TICKS(IP_TIMEOUT_MS));
    if (bits & IP_OK_BIT) {
        ESP_LOGI(TAG, "已连接 %s", app_cfg_ssid());
        return ESP_OK;
    }
    return (bits & FAIL_BIT) ? APP_ERR_WIFI_AUTH_FAIL : APP_ERR_WIFI_NO_IP;
}

esp_err_t app_wifi_connect(void)
{
    // 阻塞连接期间阶梯补发必须闭嘴:两边同时动事件位会把"已连上"判成失败。
    s_manual = true;
    const esp_err_t err = connect_blocking();
    s_manual = false;
    return err;
}

// 定向扫描一张已记住的网络:驱动按 SSID 过滤,单个网络十几毫秒。扫到返回
// rssi,扫不到返回 -128;-128 在有效 rssi(-100..-20)之外,不会被误读成"信号
// 弱但存在"。与全量扫描同样会被"连接态"拒绝 —— 失败一律按 -128 上报,由调用方
// 决定要不要先还射频空闲。
static int8_t wifi_scan_ssid(const char *ssid)
{
    if (!s_started || ssid == NULL || ssid[0] == '\0') return -128;
    wifi_scan_config_t cfg = { 0 };
    strlcpy((char *)cfg.ssid, ssid, sizeof(cfg.ssid));
    cfg.show_hidden = false;
    cfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    cfg.scan_time.active.min = 20;
    cfg.scan_time.active.max = 100;

    s_scan_busy = true;
    esp_err_t err = esp_wifi_scan_start(&cfg, true);
    s_scan_busy = false;
    if (err != ESP_OK) return -128;

    int8_t best = -128;
    wifi_ap_record_t rec;
    while (esp_wifi_scan_get_ap_record(&rec) == ESP_OK) {
        if (strcmp((const char *)rec.ssid, ssid) == 0 && rec.rssi > best)
            best = rec.rssi;
    }
    (void)esp_wifi_clear_ap_list();
    return best;
}

// 选网扫描:在已记住的多张网里挑信号最强的作为活动配对。三层语义,上层照着用:
//   true = 选网完成:app_cfg 里就是该连的网(可能还是原网络,或一张都不在附近,
//          这两种情况按当前配置直连即可);
//   false = 选网被状态挡住(见下),本轮不该拿旧网盲连。
// 挡住的根因是实测结论:STA 带着"已在别处"的旧网进入连接循环后,驱动一直在
// 空扫那一个 SSID,期间任何 esp_wifi_scan_start 都被打回 ESP_ERR_WIFI_STATE,
// 选网窗口永远开不了。所以:允许重建时先把整栈推回空闲(新栈上没有连接循环,
// 扫描必成),原地重扫;局域网配置会话在场不许重建(会拆 httpd/mDNS),退化为
// "清一次连接 + 等一拍"再扫。
static bool scan_pick(bool allow_rebuild)
{
    if (app_cfg_wifi_count() <= 1u) return true;   // 只有一张记忆:直连最近用的,不扫

    np_scan_ap_t aps[NP_SCAN_KEEP];
    size_t n = 0;
    bool ok = (app_wifi_scan(NULL, aps, NP_SCAN_KEEP, &n) == ESP_OK);
    if (!ok) {
        if (allow_rebuild) {
            if (app_wifi_restart() != ESP_OK) return false;
        } else {
            (void)esp_wifi_disconnect();
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        ok = (app_wifi_scan(NULL, aps, NP_SCAN_KEEP, &n) == ESP_OK);
    }
    if (!ok) {
        ESP_LOGW(TAG, "选网扫描重建后仍失败,本轮不动,等下一拍");
        return false;
    }

    const int pick = app_cfg_wifi_pick(aps, n);
    if (pick >= 0) {
        (void)app_cfg_wifi_select((size_t)pick);
        ESP_LOGI(TAG, "漫游:活动网切到 %s", app_cfg_ssid());
        return true;
    }
    // 列表里一张已记住的网都没有:周围 AP 太多,弱信号的认识网被
    // NP_SCAN_KEEP 截在表外。不能拿"没扫到"当"不在附近"就盲连,逐张
    // 定向补扫,取信号最强那张。
    int best_idx = -1;
    int8_t best_rssi = -128;
    for (size_t i = 0; i < app_cfg_wifi_count(); i++) {
        const char *want = app_cfg_wifi_ssid_at(i);
        if (want == NULL || want[0] == '\0') continue;
        const int8_t r = wifi_scan_ssid(want);
        if (r > best_rssi) { best_rssi = r; best_idx = (int)i; }
    }
    if (best_idx >= 0) {
        (void)app_cfg_wifi_select((size_t)best_idx);
        ESP_LOGI(TAG, "定向补扫找到 %d dBm:活动网切到 %s", (int)best_rssi,
                 app_cfg_ssid());
    }
    return true;
}

// 选网扫描的对外出口:上层在"阻塞连接前先看附近有没有已记住的网"时用
// (见 netpanel_app 的 wifi_pick_known)。true = 当前配置即已知网,可以直接连;
// false = 选网被状态挡住,上层这一拍不要盲连。
bool app_wifi_preflight_roam(bool allow_rebuild)
{
    return scan_pick(allow_rebuild);
}

// 阶梯每一档补发前的公共一步:到频率上限就先选网(选网内部自带"扫描被拒就重建
// 空闲"的自救),选完再按当前配对发起连接。force_pick = 栈刚重建过,这一拍必须
// 选网(新栈空转不会产生连接事件,不选就永远停在断开态)。返回 false = 选网没
// 就绪,本轮不补发:拿一张注定失败的旧网冲一轮连接,只会把射频再次锁进死循环。
static bool kick_with_roam(int64_t now_ms, bool allow_rebuild, bool force_pick)
{
    if (!force_pick && app_cfg_wifi_count() > 1u &&
        now_ms - s_roam_ms >= (int64_t)ROAM_INTERVAL_MS) {
        s_roam_ms = now_ms;
        if (!scan_pick(allow_rebuild)) return false;
    }
    kick_connect();
    return true;
}

void app_wifi_reconcile(int64_t now_ms, bool allow_rebuild)
{
    if (s_manual || !s_started || !app_cfg_has_wifi()) return;
    // 扫描占用射频期间不能补发:驱动会中止扫描并回 ESP_ERR_WIFI_STATE,浏览器里
    // 那次"重新扫描"就空了。扫完 heal_after_scan() 当场补一跳,阶梯最多晚 500 毫秒。
    if (s_scan_busy) return;

    if (s_up) {
        // 关联上了却迟迟拿不到地址:DHCP 客户端不会再自己重来,只能重走连接。
        if (s_has_ip || s_no_ip_ms == 0) return;
        if (now_ms - s_no_ip_ms < (int64_t)IP_TIMEOUT_MS * 2) return;
        ESP_LOGW(TAG, "关联后 %u 秒仍无地址,重新发起连接",
                 (unsigned)(IP_TIMEOUT_MS * 2 / 1000));
        s_no_ip_ms = now_ms;
        (void)esp_wifi_disconnect();
        return;
    }

    switch (np_link_tick(&s_link, now_ms)) {
    case NP_LINK_ACT_NONE:
        break;
    case NP_LINK_ACT_CONNECT:
        (void)kick_with_roam(now_ms, allow_rebuild, false);
        break;
    case NP_LINK_ACT_FORGET:
        // 卡在"正在连接"时 esp_wifi_connect 会被原地吞掉,先拆一次再连;
        // 选网放拆后,射频才确定给学生(见 kick_with_roam)。
        ESP_LOGW(TAG, "断链补发 %u 轮未恢复,先清连接状态",
                 (unsigned)np_link_tries(&s_link));
        (void)esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(200));
        (void)kick_with_roam(now_ms, allow_rebuild, false);
        break;
    case NP_LINK_ACT_REBUILD:
        // 补发到头还不回来:事件句柄与事件组的对应关系已经不可信,整栈重建。
        // 这是"必须重走热点配网"那个死路的出口 —— 重建后的栈是空闲的,
        // 这一拍紧跟一次选网(force_pick),换到新位置的已记住网直接连,
        // 不再无条件拿旧网补发。
        if (!allow_rebuild) {
            // 但局域网配置页的 httpd 与 mDNS 就绑在这块 netif 上,拆了页面就死。
            // 这一档降级成清连接状态,链路真回来了再谈重建。
            ESP_LOGW(TAG, "断链补发 %u 轮未恢复,局域网配置页在,先不清栈",
                     (unsigned)np_link_tries(&s_link));
            (void)esp_wifi_disconnect();
            vTaskDelay(pdMS_TO_TICKS(200));
            (void)kick_with_roam(now_ms, false, false);
            break;
        }
        ESP_LOGW(TAG, "断链补发 %u 轮未恢复,整栈重建 Wi-Fi",
                 (unsigned)np_link_tries(&s_link));
        if (app_wifi_restart() != ESP_OK) break;
        // 重建把事件句柄与阶梯都换了新的:驱动未必再发断开事件,阶梯不会自己
        // 武装 —— 这里补一次,下一个 3 秒档照常走选网补发。
        np_link_down(&s_link, now_ms);
        (void)kick_with_roam(now_ms, true, true);
        break;
    }
}

esp_err_t app_wifi_stop(void)
{
    if (!s_started) return ESP_OK;
    // 先停事件驱动的自动重连,再走完整的断开→停→卸载链。
    s_started = false;
    (void)esp_wifi_disconnect();
    esp_err_t err = esp_wifi_stop();
    s_up = false;
    s_has_ip = false;
    s_auth_try = 0;
    s_no_ip_ms = 0;
    if (s_events) xEventGroupClearBits(s_events, CONNECTED_BIT | FAIL_BIT | IP_OK_BIT);
    s_rssi = 0;
    if (s_connected_h) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED,
                                              s_connected_h);
        s_connected_h = NULL;
    }
    if (s_disconnected_h) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                              s_disconnected_h);
        s_disconnected_h = NULL;
    }
    if (s_got_ip_h) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_got_ip_h);
        s_got_ip_h = NULL;
    }
    esp_wifi_deinit();
    if (s_netif) {
        esp_netif_destroy(s_netif);
        s_netif = NULL;
    }
    return err;
}

// 把整个 STA 栈推回"没发起过连接"的空闲态:新栈上驱动没有对死网的连接循环,
// 扫描与选网才可能成(否则 esp_wifi_scan_start 被 ESP_ERR_WIFI_STATE 打回)。
// 阶梯一并清零 —— 调用方要么当场补 np_link_down 重新武装,要么就在补发之后的
// 下一拍靠断开事件自然武装。本函数是幂等的(未启动时等价于 init)。
esp_err_t app_wifi_restart(void)
{
    app_wifi_stop();
    if (!s_started && app_wifi_init() != ESP_OK) return ESP_FAIL;
    return ESP_OK;
}

bool app_wifi_is_up(void) { return s_up; }

bool app_wifi_has_ip(void) { return s_up && s_has_ip; }

int8_t app_wifi_rssi(void)
{
    if (!s_up) return 0;
    // 扫描期间这条驱动调用要么白跑要么报错,拿上一次的值就够了:它只是状态栏
    // 里那几段信号格,不值得在渲染锁里再压一次驱动。
    if (s_scan_busy) return s_rssi;
    wifi_ap_record_t rec;
    s_rssi = (esp_wifi_sta_get_ap_info(&rec) == ESP_OK) ? rec.rssi : 0;
    return s_rssi;
}

bool app_wifi_scan_busy(void) { return s_scan_busy; }

// 下一次补发还有多少毫秒;0 = 没有要等的(链路在、或这一拍就该发了)。
// 屏幕上那句"重连中,下次 N 秒后"就是它,好让等待看起来有人在管。
uint32_t app_wifi_retry_countdown_ms(void)
{
    if (s_up) return 0u;
    return np_link_wait_ms(&s_link, esp_timer_get_time() / 1000);
}

const char *app_wifi_ip_str(char *out, size_t cap)
{
    if (out == NULL || cap == 0u) return "";
    out[0] = '\0';
    ip4_addr_t ip = { 0 };
    esp_netif_ip_info_t info = { 0 };
    if (s_netif && esp_netif_get_ip_info(s_netif, &info) == ESP_OK) {
        ip.addr = info.ip.addr;
    }
    // 没有地址就交空串:屏幕上"0.0.0.0"看着像成功,实际谁都打不开。
    if (ip.addr == 0u) return out;
    snprintf(out, cap, IPSTR, IP2STR(&ip));
    return out;
}

// 一次扫描的原始记录上限:驱动最多存 40 条,这里只取信号最好的前若干条,
// 剩下的用 esp_wifi_clear_ap_list() 释放,不留到下次扫描。
#define SCAN_RAW_MAX 24

// 扫描期间掉链是局域网配置页的常见副作用(全信道主动扫描要把射频借走)。
// 驱动只在断开事件那一刻自己重连一次,那次请求很可能正被扫描吞掉,所以扫完
// 立刻自己看一眼:链路没了就当场补发,别等上层阶梯。
static void heal_after_scan(bool was_up)
{
    if (!was_up || s_up || !s_started) return;
    ESP_LOGW(TAG, "扫描后链路已断,立即补发连接");
    kick_connect();
}

esp_err_t app_wifi_scan(const char *exclude_ssid, np_scan_ap_t *out,
                        size_t out_max, size_t *count)
{
    if (out == NULL || count == NULL || out_max == 0u) return ESP_ERR_INVALID_ARG;
    *count = 0u;
    const bool was_up = s_up;

    wifi_scan_config_t cfg = { 0 };
    cfg.show_hidden = false;
    cfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    cfg.scan_time.active.min = 60;
    cfg.scan_time.active.max = 200;

    const int64_t t0 = esp_timer_get_time();
    // 扫描是同步阻塞的,这一个调用就是射频被借走的整段时间:标志贴着它放,
    // 结果怎么回来都不必为提前返回补记账。
    s_scan_busy = true;
    esp_err_t err = esp_wifi_scan_start(&cfg, true);
    s_scan_busy = false;
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "扫描失败 %s", esp_err_to_name(err));
        return err;
    }

    uint16_t total = 0;
    err = esp_wifi_scan_get_ap_num(&total);
    if (err != ESP_OK) {
        (void)esp_wifi_clear_ap_list();
        heal_after_scan(was_up);
        return err;
    }

    np_scan_ap_t raw[SCAN_RAW_MAX];
    size_t got = 0u;
    wifi_ap_record_t rec;
    while (got < SCAN_RAW_MAX && esp_wifi_scan_get_ap_record(&rec) == ESP_OK) {
        const size_t len = strnlen((const char *)rec.ssid, NP_SCAN_SSID_MAX);
        if (len == 0u) continue;
        if (exclude_ssid && strncmp((const char *)rec.ssid, exclude_ssid, len) == 0 &&
            exclude_ssid[len] == '\0') {
            continue;
        }
        raw[got].rssi = rec.rssi;
        raw[got].open = (rec.authmode == WIFI_AUTH_OPEN);
        memcpy(raw[got].ssid, rec.ssid, len);
        raw[got].ssid[len] = '\0';
        got++;
    }
    (void)esp_wifi_clear_ap_list();

    *count = np_scanlist_dedupe(raw, got, out, out_max);
    ESP_LOGI(TAG, "扫描到 %u 条,返回 %u 条,耗时 %lldms", (unsigned)total,
             (unsigned)*count, (long long)((esp_timer_get_time() - t0) / 1000));
    heal_after_scan(was_up);
    return ESP_OK;
}
