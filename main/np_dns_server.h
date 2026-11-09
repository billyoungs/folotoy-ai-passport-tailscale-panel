// main/np_dns_server.h —— 引导热点的门户 DNS 劫持(所有 A 查询回答热点自身 IP)。
#pragma once

#include <stdbool.h>

// 在指定 netif(如 "WIFI_AP_DEF")上起 53 端口 UDP 服务。已在跑则返回 false。
bool np_dns_start(const char *if_key);

// 停止并等待任务退出(内部用回环包唤醒阻塞的 recvfrom)。
void np_dns_stop(void);

bool np_dns_running(void);
