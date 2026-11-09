// main/np_mem.c —— 遥测文案组装(全 ASCII:不往字形清表里塞东西)。
#include "np_mem.h"

#include <stdio.h>

void np_mem_format(const np_mem_snap_t *s, char *out, size_t cap)
{
    if (out == NULL || cap == 0u) return;
    out[0] = '\0';
    if (s == NULL) return;

    const int n = snprintf(out, cap, "heap=%d blk=%d dma=%d/%d", s->free_heap,
                           s->largest_blk, s->dma_free, s->dma_blk);
    // 堆那半截都被挤掉了,就不再接 LVGL:snprintf 的返回值是"想写的长度",
    // 大于等于 cap 说明已经截断,继续拼只会把已有内容挤成乱码。
    if (n < 0 || (size_t)n >= cap) return;
    if (s->lv_total == 0u) return;

    (void)snprintf(out + n, cap - (size_t)n,
                   " lv_free=%u lv_blk=%u lv_max=%u lv_pct=%u%% lv_frag=%u%%",
                   s->lv_free, s->lv_biggest, s->lv_max_used, s->lv_used_pct,
                   s->lv_frag_pct);
}
