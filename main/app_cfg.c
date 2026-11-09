// main/app_cfg.c —— 配置全部驻留 RAM,启动时从 NVS 读入,保存时写回。
#include "app_cfg.h"

#include "esp_log.h"
#include "nvs.h"

#include "np_display_cfg.h"
#include "np_wifi_store.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "app_cfg";
#define CFG_NS "np"

static char s_ssid[33];
static char s_psk[65];
static char s_tailnet[65];
static char s_token[256];
static char s_oauth_id[129];
static char s_oauth_secret[129];
static np_auth_type_t s_auth_type;
static uint32_t s_poll_base_s = 120;
static uint32_t s_poll_cap_s = 3600;
static int32_t s_tz_offset_s = 8 * 3600;
// 显示设置:亮度(1..100)、熄屏间隔(分钟,0=从不)、熄屏显示开关。
static uint8_t s_brightness = 100;
static uint8_t s_off_minutes = 10;
static bool s_aod_enabled = true;
// 配过的 Wi-Fi 都记着;活动配对(s_ssid/s_psk)与 0 号槽在保存时对齐。
static np_wifi_store_t s_wifi_store;

static void read_str(nvs_handle_t h, const char *key, char *buf, size_t cap)
{
    buf[0] = '\0';
    size_t len = cap;
    // nvs_get_str 在缓冲区不够时返回 ESP_ERR_NVS_NOT_ENOUGH_SPACE 且不写入,
    // 等价于"该字段作废",先置空即可。
    (void)nvs_get_str(h, key, buf, &len);
}

static void write_str(nvs_handle_t h, const char *key, const char *val)
{
    if (val[0] != '\0') {
        (void)nvs_set_str(h, key, val);
    } else {
        (void)nvs_erase_key(h, key);
    }
}

esp_err_t app_cfg_load(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(CFG_NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) { // 首次开机,全用默认值
        return ESP_OK;
    }
    if (err != ESP_OK) return err;

    read_str(h, "ssid", s_ssid, sizeof(s_ssid));
    read_str(h, "psk", s_psk, sizeof(s_psk));
    read_str(h, "tailnet", s_tailnet, sizeof(s_tailnet));
    read_str(h, "token", s_token, sizeof(s_token));
    read_str(h, "oid", s_oauth_id, sizeof(s_oauth_id));
    read_str(h, "osec", s_oauth_secret, sizeof(s_oauth_secret));
    uint16_t at = 0;
    if (nvs_get_u16(h, "auth", &at) == ESP_OK && at <= NP_AUTH_OAUTH)
        s_auth_type = (np_auth_type_t)at;
    uint32_t v;
    if (nvs_get_u32(h, "base", &v) == ESP_OK && v >= 30u) s_poll_base_s = v;
    if (nvs_get_u32(h, "cap", &v) == ESP_OK && v >= 60u) s_poll_cap_s = v;
    int32_t tz;
    if (nvs_get_i32(h, "tz", &tz) == ESP_OK && tz >= -14 * 3600 && tz <= 14 * 3600)
        s_tz_offset_s = tz;
    uint8_t bri = 0;
    if (nvs_get_u8(h, "bri", &bri) == ESP_OK && np_display_cfg_valid_brightness(bri))
        s_brightness = bri;
    uint8_t offm = 0;
    if (nvs_get_u8(h, "offm", &offm) == ESP_OK && np_display_cfg_valid_off_minutes(offm))
        s_off_minutes = offm;
    uint8_t aod = 0;
    if (nvs_get_u8(h, "aod", &aod) == ESP_OK) s_aod_enabled = (aod != 0u);
    uint8_t wn = 0;
    if (nvs_get_u8(h, "wn", &wn) == ESP_OK && wn > NP_WIFI_MAX) wn = NP_WIFI_MAX;
    for (uint8_t i = 0; i < wn; i++) {
        char key[8];
        char ssid[sizeof(s_ssid)];
        char psk[sizeof(s_psk)];
        snprintf(key, sizeof(key), "w%usid", (unsigned)i);
        read_str(h, key, ssid, sizeof(ssid));
        snprintf(key, sizeof(key), "w%upsk", (unsigned)i);
        read_str(h, key, psk, sizeof(psk));
        if (ssid[0] != '\0') np_wifi_store_upsert(&s_wifi_store, ssid, psk);
    }
    // 旧固件只有 ssid/psk 一对键:首次升版把它们带进多网络记忆(仅内存,
    // 等下次保存才落成 w* 键),这样不重新配网也能记住原有那张网。
    if (s_wifi_store.count == 0u && s_ssid[0] != '\0')
        np_wifi_store_upsert(&s_wifi_store, s_ssid, s_psk);
    nvs_close(h);
    ESP_LOGI(TAG, "配置载入:ssid=%s tailnet=%s auth=%d",
             s_ssid[0] ? s_ssid : "(空)", s_tailnet[0] ? s_tailnet : "-",
             (int)s_auth_type);
    return ESP_OK;
}

esp_err_t app_cfg_save(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(CFG_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    // 活动配对进记忆并置顶:这次连的网就是最近使用的那个。
    if (s_ssid[0] != '\0') np_wifi_store_upsert(&s_wifi_store, s_ssid, s_psk);
    write_str(h, "ssid", s_ssid);
    write_str(h, "psk", s_psk);
    write_str(h, "tailnet", s_tailnet);
    write_str(h, "token", s_token);
    write_str(h, "oid", s_oauth_id);
    write_str(h, "osec", s_oauth_secret);
    (void)nvs_set_u16(h, "auth", (uint16_t)s_auth_type);
    (void)nvs_set_u32(h, "base", s_poll_base_s);
    (void)nvs_set_u32(h, "cap", s_poll_cap_s);
    (void)nvs_set_i32(h, "tz", s_tz_offset_s);
    (void)nvs_set_u8(h, "bri", s_brightness);
    (void)nvs_set_u8(h, "offm", s_off_minutes);
    (void)nvs_set_u8(h, "aod", s_aod_enabled ? 1u : 0u);
    (void)nvs_set_u8(h, "wn", (uint8_t)s_wifi_store.count);
    for (size_t i = 0; i < NP_WIFI_MAX; i++) {
        char key[8];
        snprintf(key, sizeof(key), "w%usid", (unsigned)i);
        write_str(h, key, i < s_wifi_store.count ? s_wifi_store.slot[i].ssid : "");
        snprintf(key, sizeof(key), "w%upsk", (unsigned)i);
        write_str(h, key, i < s_wifi_store.count ? s_wifi_store.slot[i].psk : "");
    }
    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

bool app_cfg_has_wifi(void) { return s_ssid[0] != '\0' && s_psk[0] != '\0'; }

bool app_cfg_has_tailscale(void)
{
    if (s_auth_type == NP_AUTH_OAUTH)
        return s_oauth_id[0] != '\0' && s_oauth_secret[0] != '\0';
    return s_token[0] != '\0';
}

bool app_cfg_any_credential(void)
{
    return app_cfg_has_wifi() || app_cfg_has_tailscale();
}

const char *app_cfg_ssid(void) { return s_ssid; }
const char *app_cfg_psk(void) { return s_psk; }
const char *app_cfg_tailnet(void) { return s_tailnet; }
np_auth_type_t app_cfg_auth_type(void) { return s_auth_type; }
const char *app_cfg_access_token(void) { return s_token; }
const char *app_cfg_oauth_id(void) { return s_oauth_id; }
const char *app_cfg_oauth_secret(void) { return s_oauth_secret; }
uint32_t app_cfg_poll_base_s(void) { return s_poll_base_s; }
uint32_t app_cfg_poll_cap_s(void) { return s_poll_cap_s; }
int32_t app_cfg_tz_offset_s(void) { return s_tz_offset_s; }

static void copy_bounded(const char *src, char *dst, size_t cap)
{
    size_t len = strlen(src);
    if (len >= cap) {
        dst[0] = '\0'; // 超长:作废原值,不留半旧半新的混合状态
        return;
    }
    memcpy(dst, src, len + 1);
}

bool app_cfg_set_ssid(const char *ssid)
{
    if (ssid == NULL) return false;
    const size_t len = strlen(ssid);
    if (len == 0 || len >= sizeof(s_ssid)) return false;
    memcpy(s_ssid, ssid, len + 1);
    return true;
}

bool app_cfg_set_psk(const char *psk)
{
    // Wi-Fi 密码允许 8..63;也允许空 = 开放网络。
    if (psk == NULL) return false;
    const size_t len = strlen(psk);
    if (len != 0 && (len < 8 || len >= sizeof(s_psk))) return false;
    memcpy(s_psk, psk, len + 1);
    return true;
}

bool app_cfg_set_tailnet(const char *tailnet)
{
    if (tailnet == NULL) return false;
    copy_bounded(tailnet, s_tailnet, sizeof(s_tailnet));
    return true;
}

bool app_cfg_set_auth(np_auth_type_t type, const char *a, const char *b)
{
    if (type != NP_AUTH_ACCESS_TOKEN && type != NP_AUTH_OAUTH) return false;
    char token[sizeof(s_token)] = "";
    char oid[sizeof(s_oauth_id)] = "";
    char osec[sizeof(s_oauth_secret)] = "";
    if (type == NP_AUTH_ACCESS_TOKEN) {
        if (a == NULL || a[0] == '\0' || strlen(a) >= sizeof(token)) return false;
        memcpy(token, a, strlen(a) + 1);
    } else {
        if (a == NULL || b == NULL || a[0] == '\0' || b[0] == '\0') return false;
        if (strlen(a) >= sizeof(oid) || strlen(b) >= sizeof(osec)) return false;
        memcpy(oid, a, strlen(a) + 1);
        memcpy(osec, b, strlen(b) + 1);
    }
    s_auth_type = type;
    memcpy(s_token, token, sizeof(s_token));
    memcpy(s_oauth_id, oid, sizeof(s_oauth_id));
    memcpy(s_oauth_secret, osec, sizeof(s_oauth_secret));
    return true;
}

bool app_cfg_set_token(const char *token)
{
    if (token == NULL || strlen(token) >= sizeof(s_token)) return false;
    memcpy(s_token, token, strlen(token) + 1);
    return true;
}

bool app_cfg_set_oauth(const char *client_id, const char *client_secret)
{
    if (client_id == NULL || client_secret == NULL) return false;
    if (strlen(client_id) >= sizeof(s_oauth_id)) return false;
    if (strlen(client_secret) >= sizeof(s_oauth_secret)) return false;
    memcpy(s_oauth_id, client_id, strlen(client_id) + 1);
    memcpy(s_oauth_secret, client_secret, strlen(client_secret) + 1);
    return true;
}

void app_cfg_set_poll(uint32_t base_s, uint32_t cap_s)
{
    if (base_s < 30u || cap_s < base_s || base_s > 86400u || cap_s > 86400u) return;
    s_poll_base_s = base_s;
    s_poll_cap_s = cap_s;
}

void app_cfg_set_tz_offset(int32_t offset_s)
{
    if (offset_s < -14 * 3600 || offset_s > 14 * 3600) return;
    s_tz_offset_s = offset_s;
}

uint8_t app_cfg_brightness(void) { return s_brightness; }
uint8_t app_cfg_off_minutes(void) { return s_off_minutes; }
bool app_cfg_aod_enabled(void) { return s_aod_enabled; }

bool app_cfg_set_brightness(uint8_t v)
{
    if (!np_display_cfg_valid_brightness(v)) return false;
    s_brightness = v;
    return true;
}

bool app_cfg_set_off_minutes(uint8_t minutes)
{
    if (!np_display_cfg_valid_off_minutes(minutes)) return false;
    s_off_minutes = minutes;
    return true;
}

bool app_cfg_set_aod_enabled(bool on)
{
    s_aod_enabled = on;
    return true;
}

void app_cfg_clear_tailscale(void)
{
    s_token[0] = '\0';
    s_oauth_id[0] = '\0';
    s_oauth_secret[0] = '\0';
}

void app_cfg_clear_wifi(void)
{
    s_ssid[0] = '\0';
    s_psk[0] = '\0';
}

size_t app_cfg_wifi_count(void) { return s_wifi_store.count; }

bool app_cfg_wifi_select(size_t idx)
{
    if (!np_wifi_store_select(&s_wifi_store, idx)) return false;
    // 置顶后 0 号就是刚选中的:复制为活动配对。落盘等下次保存。
    memcpy(s_ssid, s_wifi_store.slot[0].ssid, sizeof(s_ssid));
    memcpy(s_psk, s_wifi_store.slot[0].psk, sizeof(s_psk));
    return true;
}

int app_cfg_wifi_pick(const np_scan_ap_t *aps, size_t n)
{
    return np_wifi_store_pick(&s_wifi_store, aps, n);
}

void app_cfg_mask(const char *secret, char *out, size_t out_cap)
{
    if (out == NULL || out_cap == 0u) return;
    out[0] = '\0';
    if (secret == NULL || secret[0] == '\0') {
        snprintf(out, out_cap, "未设置");
        return;
    }
    const size_t len = strlen(secret);
    if (len < 7u) {
        snprintf(out, out_cap, "已设置");
        return;
    }
    snprintf(out, out_cap, "…%.4s", secret + len - 4u);
}
