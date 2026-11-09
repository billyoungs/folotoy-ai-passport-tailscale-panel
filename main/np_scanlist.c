// main/np_scanlist.c —— 附近热点列表逻辑。
#include "np_scanlist.h"

#include "np_json.h"

#include <string.h>

static bool stronger(const np_scan_ap_t *a, const np_scan_ap_t *b)
{
    if (a->rssi != b->rssi) return a->rssi > b->rssi;
    return strcmp(a->ssid, b->ssid) < 0;
}

static void swap_ap(np_scan_ap_t *x, np_scan_ap_t *y)
{
    np_scan_ap_t t = *x;
    *x = *y;
    *y = t;
}

// 新元素放到 idx 后往前冒泡,保证列表始终有序,免去第二次排序。
static void bubble_up(np_scan_ap_t *out, size_t idx)
{
    while (idx > 0 && stronger(&out[idx], &out[idx - 1])) {
        swap_ap(&out[idx], &out[idx - 1]);
        idx--;
    }
}

size_t np_scanlist_dedupe(const np_scan_ap_t *in, size_t n,
                          np_scan_ap_t *out, size_t out_max)
{
    if (in == NULL || out == NULL || out_max == 0u) return 0u;

    size_t cnt = 0u;
    for (size_t i = 0u; i < n; i++) {
        if (in[i].ssid[0] == '\0') continue;

        size_t j = 0u;
        for (; j < cnt; j++) {
            if (strcmp(out[j].ssid, in[i].ssid) == 0) break;
        }
        if (j < cnt) {
            if (stronger(&in[i], &out[j])) {
                out[j].rssi = in[i].rssi;
                out[j].open = in[i].open;
                bubble_up(out, j);
            }
            continue;
        }
        if (cnt < out_max) {
            out[cnt] = in[i];
            bubble_up(out, cnt);
            cnt++;
        } else if (stronger(&in[i], &out[out_max - 1])) {
            out[out_max - 1] = in[i];
            bubble_up(out, out_max - 1);
        }
    }
    return cnt;
}

bool np_scanlist_json(const np_scan_ap_t *aps, size_t n, const char *saved,
                      char *out, size_t cap)
{
    if (out == NULL || (aps == NULL && n > 0u)) return false;

    size_t u = 0u;
    if (!np_json_lit(out, cap, &u, "{\"aps\":[")) return false;
    for (size_t i = 0u; i < n; i++) {
        if (i > 0u && !np_json_lit(out, cap, &u, ",")) return false;
        if (!np_json_lit(out, cap, &u, "{\"ssid\":\"")) return false;
        if (!np_json_str(out, cap, &u, aps[i].ssid)) return false;
        if (!np_json_lit(out, cap, &u, "\",\"rssi\":")) return false;
        if (!np_json_int(out, cap, &u, (int)aps[i].rssi)) return false;
        if (!np_json_lit(out, cap, &u, aps[i].open ? ",\"open\":true}"
                                                 : ",\"open\":false}")) return false;
    }
    if (!np_json_lit(out, cap, &u, "],\"saved\":\"")) return false;
    if (!np_json_str(out, cap, &u, saved ? saved : "")) return false;
    return np_json_lit(out, cap, &u, "\"}");
}
