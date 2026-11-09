// main/np_wifi_store.h —— 已记住 Wi-Fi 的多网络存储(纯逻辑,主机可测)。
// 设备会被带到不同环境:最后配的那张网不能覆盖以往的记忆。
// 槽位按"最近使用"排序,0 号是最近连过的;SSID 是唯一键,超出容量时
// 淘汰排在最末尾(最久没用)的那个。持久化由 app_cfg 负责,这里只管运算。
#pragma once
#include <stdbool.h>
#include <stddef.h>

#include "np_scanlist.h"

#define NP_WIFI_MAX    6
#define NP_WIFI_PSK_LEN 65   // WPA2 口令最长 64 字符,含结尾 NUL

typedef struct {
    char ssid[NP_SCAN_SSID_MAX + 1];
    char psk[NP_WIFI_PSK_LEN];
} np_wifi_net_t;

typedef struct {
    np_wifi_net_t slot[NP_WIFI_MAX];
    size_t count;
} np_wifi_store_t;

void np_wifi_store_init(np_wifi_store_t *st);

// 按 SSID 插入或更新,并把该网络置顶。空 SSID 忽略;psk 允许空 = 开放网络。
void np_wifi_store_upsert(np_wifi_store_t *st, const char *ssid, const char *psk);

// 取第 idx 个(0 = 最近用过)并在原地把它置顶。idx 越界返回 false。
bool np_wifi_store_select(np_wifi_store_t *st, size_t idx);

// 扫描结果与记忆取交集,返回命中的槽位下标。扫描结果已按信号降序,
// 所以遍历扫描序即"最强信号优先";一个都不在附近返回 -1。
int np_wifi_store_pick(const np_wifi_store_t *st, const np_scan_ap_t *aps,
                       size_t n);
