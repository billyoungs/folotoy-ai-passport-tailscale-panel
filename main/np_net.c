// main/np_net.c —— 子网掩码 → CIDR 前缀(纯整数,无平台依赖)。
#include "np_net.h"

#include <stdio.h>

int np_net_netmask_prefix(uint32_t netmask)
{
    if (netmask == 0u) return 0;   // 0.0.0.0 掩码 → /0,合法
    int bits = 0;
    for (int i = 31; i >= 0; i--) {
        if (netmask & (1u << i)) {
            bits++;
        } else {
            // 遇到第一个 0:它下面若还有 1,就是非连续掩码。
            const uint32_t lower = (i == 0) ? 0u : ((1u << i) - 1u);
            if (netmask & lower) return -1;
            return bits;
        }
    }
    return 32;   // 全 1 → /32
}

void np_net_format_prefix(uint32_t netmask, char *buf, size_t cap)
{
    if (buf == NULL || cap == 0u) return;
    const int p = np_net_netmask_prefix(netmask);
    if (p < 0) {
        snprintf(buf, cap, "/");
        return;
    }
    snprintf(buf, cap, "/%d", p);
}
