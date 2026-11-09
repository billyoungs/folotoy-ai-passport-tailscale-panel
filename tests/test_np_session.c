// tests/test_np_session.c —— 空闲续命 + 绝对上限的会话窗口测试。
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "np_session.h"

#define SEC(us) ((int64_t)(us) * 1000000)
#define IDLE SEC(600)
#define HARD SEC(1800)

static np_session_t fresh(int64_t idle_us, int64_t hard_us)
{
    np_session_t s;
    memset(&s, 0, sizeof(s));
    np_session_start(&s, 0, idle_us, hard_us);
    return s;
}

int main(void)
{
    // 未空闲到点不算过期;严格大于才过期。
    np_session_t s = fresh(IDLE, HARD);
    assert(!np_session_expired(&s, IDLE));
    assert(np_session_expired(&s, IDLE + 1));

    // 请求续命:填表途中不该因为"会话启动了 10 分钟"而掉线。
    s = fresh(IDLE, SEC(2400));
    for (int64_t t = SEC(300); t < SEC(1500); t += SEC(5)) {
        np_session_touch(&s, t);
        assert(!np_session_expired(&s, t));
    }
    // 停止操作后,从最后一次请求(1495s)起再空闲 600 秒才关。
    assert(!np_session_expired(&s, SEC(1495) + IDLE));
    assert(np_session_expired(&s, SEC(1495) + IDLE + 1));

    // 页面每 5 秒轮询状态也不能把窗口顶成无限:绝对上限到点即过期。
    s = fresh(IDLE, HARD);
    for (int64_t t = SEC(5); t < HARD; t += SEC(5)) {
        np_session_touch(&s, t);
        assert(!np_session_expired(&s, t));
    }
    assert(!np_session_expired(&s, HARD));
    assert(np_session_expired(&s, HARD + 1));

    // 过期后的请求既不复活会话,也不记账。
    const int64_t last = s.last_req_us;
    np_session_touch(&s, HARD + SEC(60));
    assert(s.last_req_us == last);
    assert(np_session_expired(&s, HARD + 1));

    // hard_us = 0 表示不设绝对上限,只按空闲。
    s = fresh(IDLE, 0);
    for (int64_t t = SEC(5); t < SEC(7200); t += SEC(5)) {
        np_session_touch(&s, t);
        assert(!np_session_expired(&s, t));
    }
    assert(!np_session_expired(&s, SEC(7195) + IDLE));
    assert(np_session_expired(&s, SEC(7195) + IDLE + 1));

    // 续命不会把 last_req 记到绝对上限之外。
    s = fresh(IDLE, SEC(100));
    np_session_touch(&s, SEC(500));
    assert(s.last_req_us == 0);

    // ---- 点了"保存并联网"之后的关闭排期 ----
    // 没排期时永远不到点,过期也不会替它做决定。
    s = fresh(IDLE, SEC(100));
    assert(!np_session_close_due(&s, SEC(9999)));
    np_session_touch(&s, SEC(101));
    assert(np_session_expired(&s, SEC(101)));
    assert(!np_session_close_due(&s, SEC(101)));

    // 宽限是给浏览器读完响应的时间:差一毫秒不关,到点才关。
    s = fresh(IDLE, HARD);
    np_session_request_close(&s, SEC(10), SEC(2));
    assert(!np_session_close_due(&s, SEC(12) - 1));
    assert(np_session_close_due(&s, SEC(12)));

    // 页面每 5 秒轮询一次状态,不能把已经排好的关闭一再往后推。
    s = fresh(IDLE, HARD);
    np_session_request_close(&s, SEC(10), SEC(2));
    np_session_touch(&s, SEC(11));
    assert(!np_session_close_due(&s, SEC(12) - 1));
    assert(np_session_close_due(&s, SEC(12)));

    // 重复请求(用户又点了一次保存)不改第一次的期。
    s = fresh(IDLE, HARD);
    np_session_request_close(&s, SEC(10), SEC(2));
    np_session_request_close(&s, SEC(20), SEC(60));
    assert(np_session_close_due(&s, SEC(20)));

    // 新会话不带上一轮的排期。
    s = fresh(IDLE, HARD);
    np_session_request_close(&s, SEC(10), SEC(2));
    np_session_start(&s, SEC(50), IDLE, HARD);
    assert(!np_session_close_due(&s, SEC(999)));

    return 0;
}
