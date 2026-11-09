// main/np_scanlist.h —— 附近热点去重、排序与扫描响应渲染(纯逻辑)。
// 射频调用留在 app_wifi,本文件只管"把一堆记录变成页面能直接用的列表",
// 这样同名字段、信号比较、JSON 截断都能在主机上测。
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NP_SCAN_SSID_MAX 32   // 802.11 上限,不含结尾 NUL
#define NP_SCAN_KEEP 12       // 一次最多回给页面的网络数

typedef struct {
    char ssid[NP_SCAN_SSID_MAX + 1];
    int8_t rssi;
    bool open;                // 无密码网络
} np_scan_ap_t;

// 忽略空名条目;同名只保留信号最强的一条。输出按 rssi 降序,同信号按名称升序,
// 最多 out_max 条,返回实际条数。out 会被覆盖,调用方提供 NP_SCAN_KEEP 容量即可。
size_t np_scanlist_dedupe(const np_scan_ap_t *in, size_t n,
                          np_scan_ap_t *out, size_t out_max);

// 渲染 {"aps":[{"ssid":"…","rssi":-47,"open":false},…],"saved":"…"}。
// saved 为已配置网络名,空指针按空串处理。容量不足返回 false 且 out 内容不保证完整。
bool np_scanlist_json(const np_scan_ap_t *aps, size_t n, const char *saved,
                      char *out, size_t cap);
