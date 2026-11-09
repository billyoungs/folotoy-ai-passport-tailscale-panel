// tests/test_np_link.c —— 断链补发阶梯测试(重点是"别停在断开态")。
#include <assert.h>
#include <stdint.h>

#include "np_link.h"

int main(void)
{
    np_link_t l;

    // 没断开过就永远不该有动作
    np_link_init(&l);
    assert(!l.armed);
    assert(np_link_tick(&l, 100000) == NP_LINK_ACT_NONE);

    // 断开后先让驱动自己试一轮:第一档 3 秒,到点前不动
    np_link_down(&l, 1000);
    assert(l.armed);
    assert(np_link_tick(&l, 1000) == NP_LINK_ACT_NONE);
    assert(np_link_tick(&l, 3999) == NP_LINK_ACT_NONE);
    assert(np_link_tick(&l, 4000) == NP_LINK_ACT_CONNECT);
    assert(np_link_tries(&l) == 1u);

    // 间隔逐档翻倍:3s → 6s → 12s → 24s,第 4 轮先清状态再连
    assert(np_link_tick(&l, 4000 + 6000) == NP_LINK_ACT_CONNECT);      // tries=2
    assert(np_link_tick(&l, 10000 + 12000) == NP_LINK_ACT_CONNECT);    // tries=3
    assert(np_link_tick(&l, 22000 + 24000) == NP_LINK_ACT_FORGET);     // tries=4
    assert(np_link_tries(&l) == 4u);

    // 封顶 30 秒:第 5 档起不再翻倍,但档位仍按 tries 走
    const int64_t t5 = 46000 + 30000;
    assert(np_link_tick(&l, t5) == NP_LINK_ACT_CONNECT);               // tries=5
    assert(np_link_delay_ms(&l) == NP_LINK_MAX_DELAY_MS);
    assert(np_link_tick(&l, t5 + 30000) == NP_LINK_ACT_CONNECT);       // tries=6
    assert(np_link_tick(&l, t5 + 60000) == NP_LINK_ACT_CONNECT);       // tries=7
    assert(np_link_tick(&l, t5 + 90000) == NP_LINK_ACT_REBUILD);       // tries=8
    assert(np_link_tick(&l, t5 + 120000) == NP_LINK_ACT_CONNECT);      // tries=9
    assert(np_link_tick(&l, t5 + 150000) == NP_LINK_ACT_CONNECT);      // tries=10
    assert(np_link_tick(&l, t5 + 180000) == NP_LINK_ACT_CONNECT);      // tries=11
    assert(np_link_tick(&l, t5 + 210000) == NP_LINK_ACT_FORGET);       // tries=12
    assert(np_link_tick(&l, t5 + 240000) == NP_LINK_ACT_CONNECT);      // tries=13
    assert(np_link_tick(&l, t5 + 270000) == NP_LINK_ACT_CONNECT);      // tries=14
    assert(np_link_tick(&l, t5 + 300000) == NP_LINK_ACT_CONNECT);      // tries=15
    assert(np_link_tick(&l, t5 + 330000) == NP_LINK_ACT_REBUILD);      // tries=16

    // 连上之后阶梯归零,下次断开从第一档重新开始
    np_link_up(&l);
    assert(!l.armed);
    assert(np_link_tries(&l) == 0u);
    assert(np_link_delay_ms(&l) == NP_LINK_FIRST_DELAY_MS);
    np_link_down(&l, 500000);
    assert(np_link_tick(&l, 503000) == NP_LINK_ACT_CONNECT);
    assert(np_link_tries(&l) == 1u);

    // 连发断开事件不能把档位拉回第一档(扫描期就是这样)
    const uint32_t delay_before = np_link_delay_ms(&l);
    np_link_down(&l, 503100);
    np_link_down(&l, 503200);
    assert(np_link_delay_ms(&l) == delay_before);
    assert(np_link_tries(&l) == 1u);
    assert(np_link_tick(&l, 509000) == NP_LINK_ACT_CONNECT);
    assert(np_link_tries(&l) == 2u);

    // ---- 屏幕上要能报出"下一次补发还有多久" ----
    np_link_t w;
    np_link_init(&w);
    assert(np_link_wait_ms(&w, 100000) == 0u);        // 没武装:没有下一次
    np_link_down(&w, 100000);
    assert(np_link_wait_ms(&w, 100000) == NP_LINK_FIRST_DELAY_MS);
    assert(np_link_wait_ms(&w, 100000 + NP_LINK_FIRST_DELAY_MS - 1) == 1u);
    assert(np_link_wait_ms(&w, 100000 + NP_LINK_FIRST_DELAY_MS) == 0u);  // 到点
    assert(np_link_tick(&w, 100000 + NP_LINK_FIRST_DELAY_MS) == NP_LINK_ACT_CONNECT);
    // 刚补发完,下一跳按用掉的那一档顺延(第一档 3000,之后 6000)。
    assert(np_link_wait_ms(&w, 103000) == 3000u);
    assert(np_link_tick(&w, 106000) == NP_LINK_ACT_CONNECT);
    assert(np_link_wait_ms(&w, 106000) == 6000u);
    np_link_up(&w);
    assert(np_link_wait_ms(&w, 200000) == 0u);        // 连上了就不再倒数

    return 0;
}
