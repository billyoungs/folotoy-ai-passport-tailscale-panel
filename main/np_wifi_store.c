// main/np_wifi_store.c —— 多网络记忆的插入/置顶/选网运算,不碰射频与 NVS。
#include "np_wifi_store.h"

#include <stdio.h>
#include <string.h>

// 把 from 位置的条目搬到 0 号,0..from-1 整体后移一格。
static void promote(np_wifi_store_t *st, size_t from)
{
    const np_wifi_net_t tmp = st->slot[from];
    for (size_t i = from; i > 0u; i--) st->slot[i] = st->slot[i - 1u];
    st->slot[0] = tmp;
}

void np_wifi_store_init(np_wifi_store_t *st)
{
    if (st == NULL) return;
    st->count = 0u;
}

void np_wifi_store_upsert(np_wifi_store_t *st, const char *ssid, const char *psk)
{
    if (st == NULL || ssid == NULL || ssid[0] == '\0') return;
    size_t hit = st->count;
    for (size_t i = 0; i < st->count; i++) {
        if (strncmp(st->slot[i].ssid, ssid, sizeof(st->slot[i].ssid)) == 0) {
            hit = i;
            break;
        }
    }
    if (hit == st->count) {
        if (st->count >= NP_WIFI_MAX) {
            hit = NP_WIFI_MAX - 1u;   // 满员:挤掉最末尾(最久没用)的
        } else {
            hit = st->count++;
        }
    }
    snprintf(st->slot[hit].ssid, sizeof(st->slot[hit].ssid), "%s", ssid);
    snprintf(st->slot[hit].psk, sizeof(st->slot[hit].psk), "%s", psk ? psk : "");
    promote(st, hit);
}

bool np_wifi_store_select(np_wifi_store_t *st, size_t idx)
{
    if (st == NULL || idx >= st->count) return false;
    if (idx != 0u) promote(st, idx);
    return true;
}

int np_wifi_store_pick(const np_wifi_store_t *st, const np_scan_ap_t *aps, size_t n)
{
    if (st == NULL || aps == NULL) return -1;
    for (size_t i = 0; i < n; i++) {
        for (size_t s = 0; s < st->count; s++) {
            if (strncmp(aps[i].ssid, st->slot[s].ssid, sizeof(st->slot[s].ssid)) == 0)
                return (int)s;
        }
    }
    return -1;
}
