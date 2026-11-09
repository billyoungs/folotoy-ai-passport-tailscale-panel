// main/np_model.c —— 数据模型与轮询退避实现。纯逻辑,无平台依赖。
#include "np_model.h"
#include <ctype.h>
#include <string.h>

void np_detail_clear(np_detail_t *d)
{
    memset(d, 0, sizeof(*d));
    d->derp_ms = -1;
}

// 大小写不敏感的子串匹配(控制面给的值是小写,但老客户端不完全守规矩)。
static bool has_ci(const char *s, const char *needle)
{
    const size_t nl = strlen(needle);
    if (nl == 0u) return true;
    for (const char *p = s; *p; p++) {
        size_t i = 0u;
        while (i < nl && p[i] != '\0' &&
               tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) {
            i++;
        }
        if (i == nl) return true;
    }
    return false;
}

uint32_t np_os_glyph(const char *os)
{
    if (os == NULL || os[0] == '\0') return 0u;
    if (has_ci(os, "linux")) return NP_GLYPH_LINUX;
    if (has_ci(os, "windows")) return NP_GLYPH_WINDOWS;
    if (has_ci(os, "darwin") || has_ci(os, "macos") || has_ci(os, "ios") ||
        has_ci(os, "apple")) {
        return NP_GLYPH_APPLE;
    }
    if (has_ci(os, "android")) return NP_GLYPH_ANDROID;
    // 走到这里说明是个报得出名字但画不出对应标志的系统:给一枚通用机器,
    // 至少名称前面有东西,不会让某几行看起来缺了一块。
    return NP_GLYPH_OTHER;
}

bool np_device_is_online(int8_t ctrl, int64_t disconnect_at)
{
    if (ctrl >= 0) return ctrl != 0;   // 控制面给了结论就用它,不猜
    return disconnect_at == 0;         // 没给才退回 lastSeen 折算
}

void np_list_clear(np_list_t *list)
{
    list->count = 0u;
    list->total = 0u;
    memset(list->items, 0, sizeof(list->items));
}

static int dev_cmp(const np_device_t *a, const np_device_t *b)
{
    if (a->online != b->online) return a->online ? -1 : 1;
    return strcmp(a->name, b->name);
}

void np_list_sort(np_list_t *list)
{
    // 数量 ≤24,插入排序足够且无递归/额外栈。
    for (size_t i = 1u; i < list->count; i++) {
        np_device_t tmp = list->items[i];
        size_t j = i;
        while (j > 0u && dev_cmp(&tmp, &list->items[j - 1u]) < 0) {
            list->items[j] = list->items[j - 1u];
            j--;
        }
        list->items[j] = tmp;
    }
}

size_t np_list_online_count(const np_list_t *list)
{
    size_t n = 0u;
    for (size_t i = 0u; i < list->count; i++) {
        if (list->items[i].online) n++;
    }
    return n;
}

int64_t np_seen_disconnect_at(int64_t now, int64_t last_seen)
{
    // 没有字段、零值时间、1970 前后这类明显不是断线时刻的值:一律当在线。
    if (last_seen < NP_TIME_FLOOR_EPOCH) return 0;
    // 时钟没回来之前没有任何绝对时刻可比,不猜。
    if (now <= 0) return 0;
    // 比 now 还新是时钟偏差,刚报过到是长连接,都算在线。
    if (now - last_seen <= NP_SEEN_GRACE_S) return 0;
    return last_seen;
}

bool np_snapshot_fresh(int64_t now_ms, int64_t fetched_ms, bool link_up,
                       uint32_t retry_delay_s)
{
    // 从没同步成功过就没有"现在"的数据可言;链路一断,上一份 online 立刻只是
    // 过去的事实,不能继续画成在线。
    if (fetched_ms == 0 || !link_up) return false;
    // 链路还在时按同步自己的窗口算:退避到上限的合法间隔不该被判成过期。
    const int64_t window = NP_SNAPSHOT_GRACE_MS + 2000LL * (int64_t)retry_delay_s;
    return now_ms - fetched_ms <= window;
}

int np_signal_bars(int8_t rssi)
{
    if (rssi == 0) return 0;                 // 未连接
    if (rssi >= -60) return 3;
    if (rssi >= -75) return 2;
    return 1;
}

void np_poll_init(np_poll_t *p, uint32_t base_s, uint32_t cap_s)
{
    p->base_s = base_s == 0u ? 120u : base_s;
    p->cap_s = cap_s < p->base_s ? p->base_s : cap_s;
    p->current_delay_s = p->base_s;
    p->next_run_s = 0;
    p->net_up = false;
}

bool np_poll_due(np_poll_t *p, int64_t now_s)
{
    if (!p->net_up) return false;
    if (now_s >= p->next_run_s) {
        p->next_run_s = now_s + (int64_t)p->current_delay_s;
        return true;
    }
    return false;
}

void np_poll_on_success(np_poll_t *p, int64_t now_s)
{
    p->current_delay_s = p->base_s;
    p->next_run_s = now_s + (int64_t)p->base_s;
}

void np_poll_on_fail(np_poll_t *p, int64_t now_s)
{
    uint32_t next = p->current_delay_s * 2u;
    if (next > p->cap_s || next < p->current_delay_s) next = p->cap_s; // 溢出也封顶
    p->current_delay_s = next;
    p->next_run_s = now_s + (int64_t)next;
}

void np_poll_set_net(np_poll_t *p, bool up)
{
    p->net_up = up;
    if (up && p->next_run_s > 0) {
        // 断网恢复后尽快重试一次,而不是等完整退避窗口。
        if (p->current_delay_s > p->base_s) p->next_run_s = 0;
    }
}

// ---------------- 双缓冲交接 ----------------
static np_list_t s_bufs[2];
static int s_active_idx;

void np_lists_init(void)
{
    np_list_clear(&s_bufs[0]);
    np_list_clear(&s_bufs[1]);
    s_bufs[0].version = 0;
    s_bufs[1].version = 0;
    s_active_idx = 0;
}

np_list_t *np_lists_active(void) { return &s_bufs[s_active_idx]; }
np_list_t *np_lists_back(void) { return &s_bufs[s_active_idx ^ 1]; }

void np_lists_swap(void) { s_active_idx ^= 1; }
