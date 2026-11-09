// main/np_clock.c —— 校时门禁节奏实现。
#include "np_clock.h"

void np_clock_init(np_clock_t *c)
{
    if (c == NULL) return;
    c->wait_start_ms = 0;
    c->next_rearm_ms = 0;
    c->rearms = 0;
    c->waiting = false;
    c->patience_shown = false;
}

np_clock_action_t np_clock_tick(np_clock_t *c, bool link_up, bool synced,
                                int64_t now_ms)
{
    if (c == NULL) return NP_CLOCK_GO;
    if (synced) { np_clock_init(c); return NP_CLOCK_GO; }
    if (!link_up) { np_clock_init(c); return NP_CLOCK_IDLE; }

    if (!c->waiting) {
        // 链路刚通、SNTP 也是这一拍前后起的,别立刻再停一次栈重启它。
        c->waiting = true;
        c->wait_start_ms = now_ms;
        c->next_rearm_ms = now_ms + NP_CLOCK_RETRY_MS;
        c->patience_shown = false;
        return NP_CLOCK_WAIT;
    }
    // 实话只说一次,重起照旧进行:用户中途放开了 UDP 123 也得能自己好起来。
    if (!c->patience_shown &&
        now_ms - c->wait_start_ms >= NP_CLOCK_PATIENCE_MS) {
        c->patience_shown = true;
        return NP_CLOCK_STALLED;
    }
    if (now_ms >= c->next_rearm_ms) {
        c->next_rearm_ms = now_ms + NP_CLOCK_RETRY_MS;
        c->rearms++;
        return NP_CLOCK_REARM;
    }
    return NP_CLOCK_WAIT;
}

bool np_clock_waiting(const np_clock_t *c)
{
    return c != NULL && c->waiting;
}

bool np_clock_stalled(const np_clock_t *c)
{
    return c != NULL && c->waiting && c->patience_shown;
}

uint32_t np_clock_rearms(const np_clock_t *c)
{
    return c ? c->rearms : 0u;
}
