// main/np_ping.h —— 即时 ping(ICMP echo)一个域名,结果只报通/不通。
// 工作任务里跑,不阻塞 GUI;GUI 线程轮询状态即可。纯网络副作用,不碰 LVGL。
#pragma once

#include <stdbool.h>

typedef enum {
    NP_PING_IDLE = 0,   // 还没测过
    NP_PING_BUSY,       // 正在检测
    NP_PING_OK,         // 收到 echo reply:通
    NP_PING_FAIL,       // 超时 / 发送失败:不通
    NP_PING_NOHOST,     // 域名解析失败(等同没有网络)
} np_ping_state_t;

// 异步启动一次 ping(工作任务里跑)。已在检测中则不重复启动,返回 false。
// host 会拷贝保存,调用方无需保持生命周期。
bool np_ping_start(const char *host);

// 当前状态(供 GUI 线程轮询)。线程安全:只读一个 volatile 枚举。
np_ping_state_t np_ping_state(void);
