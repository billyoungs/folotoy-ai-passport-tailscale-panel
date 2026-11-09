// tests/test_np_clock.c —— 校时门禁节奏测试(重点是"等,但别无限期地闷等")。
#include <assert.h>
#include <stdint.h>

#include "np_clock.h"

int main(void)
{
    np_clock_t c;

    // 钟校上了就一路放行,不留下任何等待状态
    np_clock_init(&c);
    assert(np_clock_tick(&c, true, true, 1000) == NP_CLOCK_GO);
    assert(!np_clock_waiting(&c));
    assert(np_clock_rearms(&c) == 0u);

    // 没地址时校时无从谈起:既不等待也不重起,断网那句话由链路那边负责
    np_clock_init(&c);
    assert(np_clock_tick(&c, false, false, 1000) == NP_CLOCK_IDLE);
    assert(!np_clock_waiting(&c));

    // 链路已通、钟未校上:这一拍开始等,但不立刻重启 SNTP(它刚刚才起)。
    // 等待起点 2000 ms,于是重起档落在 32000 / 62000 / 92000 ……
    np_clock_init(&c);
    assert(np_clock_tick(&c, true, false, 2000) == NP_CLOCK_WAIT);
    assert(np_clock_waiting(&c));
    assert(np_clock_tick(&c, true, false, 3000) == NP_CLOCK_WAIT);
    assert(np_clock_tick(&c, true, false, 31999) == NP_CLOCK_WAIT);

    assert(np_clock_tick(&c, true, false, 32000) == NP_CLOCK_REARM);
    assert(np_clock_rearms(&c) == 1u);
    assert(np_clock_tick(&c, true, false, 61999) == NP_CLOCK_WAIT);
    assert(np_clock_tick(&c, true, false, 62000) == NP_CLOCK_REARM);
    assert(np_clock_rearms(&c) == 2u);
    assert(np_clock_tick(&c, true, false, 92000) == NP_CLOCK_REARM);
    assert(np_clock_rearms(&c) == 3u);
    assert(np_clock_tick(&c, true, false, 92500) == NP_CLOCK_WAIT);

    // 满 120 秒仍无响应:说实话,且一轮等待里只说一次;重起照旧往下走
    assert(np_clock_tick(&c, true, false, 121999) == NP_CLOCK_WAIT);
    assert(!np_clock_stalled(&c));
    assert(np_clock_tick(&c, true, false, 122000) == NP_CLOCK_STALLED);
    assert(np_clock_rearms(&c) == 3u);      // 报老实话这一拍不占重起名额
    assert(np_clock_stalled(&c));
    assert(np_clock_tick(&c, true, false, 122001) == NP_CLOCK_REARM);
    assert(np_clock_rearms(&c) == 4u);
    assert(np_clock_tick(&c, true, false, 122501) == NP_CLOCK_WAIT);
    assert(np_clock_stalled(&c));           // 陈述状态:说过之后一直算"无响应"

    // 中途断网:等待作废,重连后重新计时,那句实话也重新有效
    assert(np_clock_tick(&c, false, false, 200000) == NP_CLOCK_IDLE);
    assert(!np_clock_stalled(&c));
    assert(np_clock_tick(&c, true, false, 200500) == NP_CLOCK_WAIT);
    assert(np_clock_tick(&c, true, false, 230499) == NP_CLOCK_WAIT);
    assert(np_clock_tick(&c, true, false, 230500) == NP_CLOCK_REARM);
    assert(np_clock_rearms(&c) == 1u);      // 新一轮从第一次重起数起
    assert(!np_clock_stalled(&c));

    // 等来校时:门禁开、全部状态归零,后面再断也不带旧账
    assert(np_clock_tick(&c, true, true, 240500) == NP_CLOCK_GO);
    assert(np_clock_rearms(&c) == 0u);
    assert(!np_clock_waiting(&c));

    // 空指针:调用方拿不到状态时按放行处理,别把设备锁死在等待里
    assert(np_clock_tick(NULL, true, false, 0) == NP_CLOCK_GO);
    np_clock_init(NULL);
    assert(!np_clock_waiting(NULL));
    assert(!np_clock_stalled(NULL));
    assert(np_clock_rearms(NULL) == 0u);

    return 0;
}
