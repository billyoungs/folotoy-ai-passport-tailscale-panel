// main/np_net.h —— 网络地址的纯逻辑(与平台无关,可主机测试)。
// 目前只有一件:子网掩码 → CIDR 前缀长度。调用方把 esp_netif 的
// netmask(网络字节序)先转成主机序再传进来,见 app_wifi.c。
#pragma once

#include <stddef.h>
#include <stdint.h>

// 主机序掩码(bit31 = 网络最高位)→ 前缀长度 0..32。
// 连续掩码(高位起连续 1)返回前缀;非连续(如 0xFF00FF00)→ -1,调用方按无效处理。
int np_net_netmask_prefix(uint32_t netmask);

// 把掩码格式化成 "/NN"(NN = 0..32);无效掩码输出 "/"。buf 始终 NUL 结尾。
void np_net_format_prefix(uint32_t netmask, char *buf, size_t cap);
