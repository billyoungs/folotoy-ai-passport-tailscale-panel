// main/np_mem.h —— 内存遥测:快照结构与一行文案的组装(纯逻辑,主机可测)。
// 为什么非要这一行:实机的"内存不足 / TLS 握手异常"要在现场分得开两种缺法 ——
// 总量不够看 free,碎片化看最大可分配块(握手要的是一块连续的记录缓冲)。
// LVGL 池是另一本账:它从 .bss 里挖走,不在系统堆里,而且饿死的表现是画面冻结,
// 不是崩溃(读表法见 docs/reference/starsms007/lvgl-pool-budget-without-psram.md)。
#pragma once

#include <stddef.h>

// 编译期钉住的池子容量。sdkconfig 里那个 CONFIG_LV_MEM_SIZE_KILOBYTES 是
// DEPRECATED 且不参与换算,np-0.1.26 之前真实值一直是 65536 而配置写着 24 ——
// 这个常量与 CONFIG_LV_MEM_SIZE 由应用层做静态断言比对,以后改配置必须同时改这里。
#define NP_LV_POOL_BYTES 65536

// 一行遥测文案的缓冲:全 ASCII,负数与满位无符号的最坏情况也放得下。
#define NP_MEM_LINE_CAP 192

typedef struct {
    int free_heap;        // esp_get_free_heap_size()
    int largest_blk;      // 最大可分配块(MALLOC_CAP_8BIT)
    int dma_free;         // DMA 区剩余(显示那条路走的是它)
    int dma_blk;          // DMA 区最大可分配块
    unsigned lv_total;    // 0 = 这一次没采到 LVGL(没拿到锁就留 0,不编数)
    unsigned lv_free;
    unsigned lv_biggest;
    unsigned lv_max_used;  // 开机以来的高水位,不是余量
    unsigned lv_used_pct;
    unsigned lv_frag_pct;
} np_mem_snap_t;

// 组装成一行:heap/blk/dma 永远打;lv_total 为 0 时整段 lv 省略(ts_api.c 那条
// 路径拿不到 LVGL 锁,只报堆的两个数也是实话)。
void np_mem_format(const np_mem_snap_t *s, char *out, size_t cap);

// 采集要遍历 LVGL 池的管理结构,只能在持 LVGL 锁的地方做,所以实现放在应用层
// (main/netpanel_app.c):本模块只保证文案组装,主机测试不依赖 IDF 与 LVGL。
void np_mem_capture(np_mem_snap_t *s);
void np_mem_last(np_mem_snap_t *s);   // 最近一次采集的缓存,可能滞后一拍
