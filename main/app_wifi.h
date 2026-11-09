// main/app_wifi.h —— STA 连接管理:凭据来自 app_cfg,断线自动重连。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "np_scanlist.h"

// 认证/密码类失败:ESP_ERR_WIFI 段里 +16/+17 未被组件占用,取 +16 作应用码。
#define APP_ERR_WIFI_AUTH_FAIL (ESP_ERR_WIFI_BASE + 16)
// 已关联但 DHCP 没给出地址:和"连不上"不同,重试前值得让 AP 喘口气。
#define APP_ERR_WIFI_NO_IP (ESP_ERR_WIFI_BASE + 17)

// 初始化 netif + esp_wifi 并 start(不发起连接)。失败自带回滚,可重试。
esp_err_t app_wifi_init(void);

// 用 app_cfg 中的 ssid/psk 发起连接,成功意味着已关联且拿到 IPv4。
// 重复调用会以新凭据重连。未配置凭据返回 ESP_ERR_INVALID_STATE。
esp_err_t app_wifi_connect(void);

// 断开并停止 Wi-Fi(释放射频,供 SoftAP 引导模式接管)。
esp_err_t app_wifi_stop(void);

bool app_wifi_is_up(void);  // 已关联(信号强度查询用)
bool app_wifi_has_ip(void); // 已关联且拿到 IPv4:局域网配置服务的前提
// 断链自愈节拍,由上层按固定周期(约 500ms)喂当前 esp_timer 毫秒数。
// 驱动只在断开事件那一刻重连一次,那一次可能被扫描吞掉或撞上"正在连接";
// 这里负责补发,并在补发到头后清状态、整栈重建。未启动/正在阻塞连接时为空操作;
// 扫描占用射频期间也直接返回(那时补发会把扫描掐掉并让驱动回
// ESP_ERR_WIFI_STATE),扫完由 heal_after_scan 当场补一跳。
// allow_rebuild=false 时最高那档降为清连接状态:局域网配置页的 httpd 与 mDNS
// 绑在这块 netif 上,整栈重建会把会话一起拆掉。
void app_wifi_reconcile(int64_t now_ms, bool allow_rebuild);
int8_t app_wifi_rssi(void); // 未连接返回 0
bool app_wifi_scan_busy(void); // 前台扫描正在占用射频
// 断链时下一次补发还有多少毫秒(0 = 不等:链路在,或这一拍就该发)。屏幕用它
// 说一句"重连中,下次 N 秒后",让等待看起来有人在管,而不是安静地卡着。
uint32_t app_wifi_retry_countdown_ms(void);
// IPv4 点分十进制;尚未取得地址时返回空串(调用方据此不显示地址行)。
const char *app_wifi_ip_str(char *out, size_t cap);

// 前台阻塞扫描附近热点(13 信道,约 1-2 秒),去重排序后写入 out,*count 为条数。
// 驱动只在含 STA 的模式下允许扫描:引导热点必须以 APSTA 起射频,纯 AP 会返回
// ESP_ERR_WIFI_MODE。exclude_ssid 用于滤掉设备自己的热点名,可为 NULL。
esp_err_t app_wifi_scan(const char *exclude_ssid, np_scan_ap_t *out,
                        size_t out_max, size_t *count);
