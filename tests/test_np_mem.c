// tests/test_np_mem.c —— 遥测行的组装测试(纯 ASCII,不涉及字形清表)。
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "np_mem.h"

static np_mem_snap_t realistic(void)
{
    np_mem_snap_t s;
    s.free_heap = 52144;
    s.largest_blk = 30720;
    s.dma_free = 19200;
    s.dma_blk = 19200;
    s.lv_total = 65536;
    s.lv_free = 61000;
    s.lv_biggest = 60928;
    s.lv_max_used = 8192;
    s.lv_used_pct = 7;
    s.lv_frag_pct = 3;
    return s;
}

int main(void)
{
    char line[NP_MEM_LINE_CAP];

    // ---- 采到 LVGL:堆三个数 + 池子五个数,一格都不能少
    np_mem_snap_t s = realistic();
    np_mem_format(&s, line, sizeof(line));
    assert(strcmp(line, "heap=52144 blk=30720 dma=19200/19200 "
                        "lv_free=61000 lv_blk=60928 lv_max=8192 lv_pct=7% "
                        "lv_frag=3%") == 0);

    // ---- 没采到 LVGL(lv_total=0):整段省略,而不是打一串 0 冒充实测值
    s.lv_total = 0u;
    s.lv_free = 999u;
    np_mem_format(&s, line, sizeof(line));
    assert(strcmp(line, "heap=52144 blk=30720 dma=19200/19200") == 0);
    assert(strstr(line, "lv_") == NULL);

    // ---- 最坏的数也不能顶破缓冲:全用极限值
    np_mem_snap_t worst;
    worst.free_heap = INT_MIN;
    worst.largest_blk = INT_MIN;
    worst.dma_free = INT_MIN;
    worst.dma_blk = INT_MIN;
    worst.lv_total = UINT_MAX;
    worst.lv_free = UINT_MAX;
    worst.lv_biggest = UINT_MAX;
    worst.lv_max_used = UINT_MAX;
    worst.lv_used_pct = UINT_MAX;
    worst.lv_frag_pct = UINT_MAX;
    char guard[NP_MEM_LINE_CAP + 8u];
    memset(guard, 0x5A, sizeof(guard));
    np_mem_format(&worst, guard, NP_MEM_LINE_CAP);
    assert(strlen(guard) < NP_MEM_LINE_CAP);
    assert(guard[NP_MEM_LINE_CAP] == 0x5A);        // 一个字节都没写出界
    assert(guard[NP_MEM_LINE_CAP + 1u] == 0x5A);

    // ---- 小缓冲:截断只许发生在结尾,已有内容不许被挤掉
    char tiny[24];
    memset(tiny, 0x5A, sizeof(tiny));
    s = realistic();
    np_mem_format(&s, tiny, sizeof(tiny));
    assert(strlen(tiny) == sizeof(tiny) - 1u);
    np_mem_format(&s, line, sizeof(line));
    assert(memcmp(tiny, line, sizeof(tiny) - 1u) == 0);   // 是整行的前缀
    assert(tiny[sizeof(tiny) - 1u] == '\0');              // 终止符落在界内

    // 只剩一个字节的缓冲也得是空串,不能越界写终止符
    char one[1];
    one[0] = 0x5A;
    np_mem_format(&s, one, sizeof(one));
    assert(one[0] == '\0');

    // ---- 非法出参:out=NULL 或 cap=0 都不许动调用方的旧文案 —— 那可能是上一轮
    // 的真数,清成空串会被读成"这轮采到 0"。
    char keep[32] = "old";
    np_mem_format(&s, NULL, sizeof(keep));
    assert(strcmp(keep, "old") == 0);
    np_mem_format(&s, keep, 0u);
    assert(strcmp(keep, "old") == 0);

    // 快照本身是空指针属于调用点写错:宁可给空行,也不许把旧数当新采集打出去。
    np_mem_format(NULL, keep, sizeof(keep));
    assert(keep[0] == '\0');

    printf("np_mem: ok\n");
    return 0;
}
