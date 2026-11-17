// tests/test_np_net.c —— 子网掩码 → CIDR 前缀。
#include <assert.h>
#include <string.h>
#include "np_net.h"

int main(void)
{
    // 常见连续掩码(主机序:bit31 是网络最高位)。
    assert(np_net_netmask_prefix(0xFFFFFFFFu) == 32);
    assert(np_net_netmask_prefix(0xFFFFFF00u) == 24);
    assert(np_net_netmask_prefix(0xFFFF0000u) == 16);
    assert(np_net_netmask_prefix(0xFF000000u) == 8);
    assert(np_net_netmask_prefix(0xF0000000u) == 4);
    assert(np_net_netmask_prefix(0x00000000u) == 0);
    // 非连续掩码 → -1。
    assert(np_net_netmask_prefix(0xFF00FF00u) == -1);
    assert(np_net_netmask_prefix(0x0FF00000u) == -1);

    char buf[8];
    np_net_format_prefix(0xFFFFFF00u, buf, sizeof(buf));
    assert(strcmp(buf, "/24") == 0);
    np_net_format_prefix(0x00000000u, buf, sizeof(buf));
    assert(strcmp(buf, "/0") == 0);
    np_net_format_prefix(0xFFFFFFFFu, buf, sizeof(buf));
    assert(strcmp(buf, "/32") == 0);
    np_net_format_prefix(0xFF00FF00u, buf, sizeof(buf));
    assert(strcmp(buf, "/") == 0);
    // 小缓冲/空指针不越界。
    np_net_format_prefix(0xFFFFFF00u, buf, 0);
    np_net_format_prefix(0xFFFFFF00u, NULL, 8);
    return 0;
}
