// main/np_ping.h —— 互联网访问探测:到固定主机做一次 TCP 连接,只报通/不通。
#pragma once

#include <stdbool.h>

typedef enum {
    NP_PING_IDLE = 0,  // 还没发起过
    NP_PING_BUSY,      // 正在探(解析/握手)
    NP_PING_OK,        // 握手成功
    NP_PING_FAIL,      // 握手失败或超时
    NP_PING_NOHOST,    // 域名解析失败
} np_ping_state_t;

// 异步发起一次探测(重复调用在她还 BUSY 时会被拒,返回 false)。
bool np_ping_start(const char *host);
np_ping_state_t np_ping_state(void);
