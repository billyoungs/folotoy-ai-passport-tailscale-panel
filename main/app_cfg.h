// main/app_cfg.h —— NVS "np" 命名空间:Wi-Fi 凭据 + Tailscale 凭据 + 轮询间隔 + 显示设置。
// 凭据只进不出:任何读取接口对外只允许掩码形式,日志绝不打印明文。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "np_scanlist.h"

typedef enum {
    NP_AUTH_ACCESS_TOKEN = 0, // tskey- 个人 API key
    NP_AUTH_OAUTH = 1,        // client_id + client_secret,换取短时 JWT
} np_auth_type_t;

esp_err_t app_cfg_load(void);
esp_err_t app_cfg_save(void); // 全字段写回;调用方保证字段已校验

bool app_cfg_has_wifi(void);        // ssid 与 psk 均非空
bool app_cfg_has_tailscale(void);   // 按 auth_type 判断所需凭据齐全
bool app_cfg_any_credential(void);  // 是否处于"已配置"状态(决定 SoftAP/LAN 引导模式)

const char *app_cfg_ssid(void);
const char *app_cfg_psk(void);
const char *app_cfg_tailnet(void);  // 空串表示用 "-"(账号默认 tailnet)
np_auth_type_t app_cfg_auth_type(void);
const char *app_cfg_access_token(void);
const char *app_cfg_oauth_id(void);
const char *app_cfg_oauth_secret(void);
uint32_t app_cfg_poll_base_s(void);
uint32_t app_cfg_poll_cap_s(void);
int32_t app_cfg_tz_offset_s(void);  // 显示用固定偏移,默认 +8h

// 多网络记忆:配过的 Wi-Fi 都记着(最近连过的排 0 号),换环境时从扫描结果里
// 挑信号最强的那个连,不用重新配网。
size_t app_cfg_wifi_count(void);
// 把第 idx 个记忆网络设为当前活动配对并置顶 MRU。只改内存,落盘等下次保存。
bool app_cfg_wifi_select(size_t idx);
// 扫描结果(已按信号降序)里找已记住的网络,返回记忆下标;没有交集返回 -1。
int app_cfg_wifi_pick(const np_scan_ap_t *aps, size_t n);

// 写入前校验/清洗。返回 false 表示超长或非法,字段保持原值。
bool app_cfg_set_ssid(const char *ssid);
bool app_cfg_set_psk(const char *psk);
bool app_cfg_set_tailnet(const char *tailnet);
bool app_cfg_set_auth(np_auth_type_t type, const char *a, const char *b);
bool app_cfg_set_token(const char *token);
bool app_cfg_set_oauth(const char *client_id, const char *client_secret);
void app_cfg_set_poll(uint32_t base_s, uint32_t cap_s);
void app_cfg_set_tz_offset(int32_t offset_s);

// 抹掉凭据但不抹 Wi-Fi(或反之)。清完后擦除对应 NVS key。
void app_cfg_clear_tailscale(void);
void app_cfg_clear_wifi(void);

// "…xxxx" 形式掩码(后 4 位)。长度不足 7 时输出 "已设置"。绝不返回完整值。
void app_cfg_mask(const char *secret, char *out, size_t out_cap);

// 显示设置:亮度(1..100)、熄屏间隔(分钟,0=从不)、熄屏显示开关。setter 非法值返回 false。
uint8_t app_cfg_brightness(void);
uint8_t app_cfg_off_minutes(void);
bool app_cfg_aod_enabled(void);
bool app_cfg_set_brightness(uint8_t v);
bool app_cfg_set_off_minutes(uint8_t minutes);
bool app_cfg_set_aod_enabled(bool on);
