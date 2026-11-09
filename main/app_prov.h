// main/app_prov.h —— 双模式 Web 配置页:SoftAP 首次引导 / LAN 凭据录入。
// 同一套页面、同一组接口;LAN 模式的写接口要求屏幕配对码,热点阶段开放
// (接入与填 Wi-Fi 不设码,np-0.2.3)。保存成功只写 NVS 并置标志,
// 重活(连接/重启服务)由上层在会话结束后做。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum {
    NP_PROV_OFF = 0,
    NP_PROV_SOFTAP, // 热点引导:开放热点,屏显热点名
    NP_PROV_LAN,    // 局域网修改:netpanel.local + 6 位配对码
} np_prov_mode_t;

// 会话窗口:距最后一次请求空闲 10 分钟,自启动起绝对上限 30 分钟。本文件不自己
// 收尾 —— 只把"该关了"算出来,由 app 任务取走并关掉服务、决定后续状态。这样
// 全程只有 np_app 一个任务开关会话,不需要等任何别的任务退场。

// 启动配置服务(非阻塞)。SoftAP 模式接管射频前会自行 deinit 现有 STA
// (esp_wifi_deinit),LAN 模式要求 STA 已连接。
// ap_ssid 仅 SoftAP 模式回填;pairing 仅 LAN 模式回填(凭据录入页的写闸门)。
esp_err_t app_prov_start(np_prov_mode_t mode, char *ap_ssid_out, size_t cap,
                         char *pairing_out, size_t pairing_cap);

void app_prov_stop(void);
np_prov_mode_t app_prov_mode(void);

// 会话是否该结束了(读后不拆;现在由谁过期、何时过期都在这里算):
// 空闲/绝对上限到点,或页面上点了"保存并联网"且浏览器读完响应的宽限已过。
bool app_prov_take_close(int64_t now_us);

// 会话结束时哪个配置段变更过(上层据此决定要不要重连 Wi-Fi)。
bool app_prov_wifi_applied(void);
bool app_prov_ts_applied(void);

// 网页上的"立即同步"按钮:取走一次请求(读后清零,UI 任务写、app 任务读)。
bool app_prov_take_sync_request(void);
