// main/np_session.c —— 见 np_session.h。时间一律用 esp_timer_get_time() 的单调
// 微秒,调用方传入,本模块不碰时钟,所以主机可测。
#include "np_session.h"

void np_session_start(np_session_t *s, int64_t now_us, int64_t idle_us,
                      int64_t hard_us)
{
    s->started_us = now_us;
    s->last_req_us = now_us;
    s->idle_us = idle_us;
    s->hard_us = hard_us;
    s->close_at_us = 0;
}

bool np_session_expired(const np_session_t *s, int64_t now_us)
{
    if (s->hard_us > 0 && now_us - s->started_us > s->hard_us) return true;
    return now_us - s->last_req_us > s->idle_us;
}

void np_session_touch(np_session_t *s, int64_t now_us)
{
    if (np_session_expired(s, now_us)) return; // 已判死刑的窗口不复活
    const int64_t ceiling = s->hard_us > 0 ? s->started_us + s->hard_us
                                           : INT64_MAX;
    s->last_req_us = now_us < ceiling ? now_us : ceiling;
}

void np_session_request_close(np_session_t *s, int64_t now_us, int64_t grace_us)
{
    if (s->close_at_us != 0) return;   // 已排期:后续请求不能把它一再推后
    s->close_at_us = now_us + grace_us;
}

bool np_session_close_due(const np_session_t *s, int64_t now_us)
{
    return s->close_at_us != 0 && now_us >= s->close_at_us;
}
