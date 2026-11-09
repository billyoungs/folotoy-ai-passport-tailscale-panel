// main/np_link.c —— 补发阶梯实现。
#include "np_link.h"

void np_link_init(np_link_t *l)
{
    l->next_try_ms = 0;
    l->delay_ms = NP_LINK_FIRST_DELAY_MS;
    l->tries = 0u;
    l->armed = false;
}

void np_link_up(np_link_t *l)
{
    np_link_init(l);
}

void np_link_down(np_link_t *l, int64_t now_ms)
{
    // 连发多个断开事件(扫描期间的重试尤其如此)不能把已经走到的档位拉回第一档,
    // 否则永远在 3 秒这一档上打转。
    if (l->armed) return;
    l->armed = true;
    l->tries = 0u;
    l->delay_ms = NP_LINK_FIRST_DELAY_MS;
    l->next_try_ms = now_ms + (int64_t)NP_LINK_FIRST_DELAY_MS;
}

np_link_action_t np_link_tick(np_link_t *l, int64_t now_ms)
{
    if (!l->armed) return NP_LINK_ACT_NONE;
    if (now_ms < l->next_try_ms) return NP_LINK_ACT_NONE;

    l->tries++;
    const uint32_t used = l->delay_ms;
    uint32_t next = used * 2u;
    if (next > NP_LINK_MAX_DELAY_MS || next < used) next = NP_LINK_MAX_DELAY_MS;
    l->delay_ms = next;
    l->next_try_ms = now_ms + (int64_t)used;

    if (l->tries % NP_LINK_REBUILD_AT == 0u) return NP_LINK_ACT_REBUILD;
    if (l->tries % NP_LINK_FORGET_AT == 0u) return NP_LINK_ACT_FORGET;
    return NP_LINK_ACT_CONNECT;
}

uint32_t np_link_delay_ms(const np_link_t *l) { return l->delay_ms; }
uint32_t np_link_tries(const np_link_t *l) { return l->tries; }

uint32_t np_link_wait_ms(const np_link_t *l, int64_t now_ms)
{
    if (!l->armed || now_ms >= l->next_try_ms) return 0u;
    const int64_t left = l->next_try_ms - now_ms;
    return left > (int64_t)UINT32_MAX ? UINT32_MAX : (uint32_t)left;
}
