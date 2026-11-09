// main/np_prompt.h —— 配置提示页文案组装(纯逻辑,主机可测)。
// 契约:第一行必须交代 Wi-Fi 结果;没有地址就不许出现地址行,任何情况下
// 都不许把 0.0.0.0 印到屏幕上。截断只落在码点边界,不留半个 UTF-8 序列。
#pragma once

#include <stdbool.h>
#include <stddef.h>

#define NP_PROMPT_LINE 40
#define NP_PROMPT_LINES 4

typedef struct {
    char line[NP_PROMPT_LINES][NP_PROMPT_LINE];
} np_prompt_t;

// 局域网配对码提示页的四行正文(顺序即屏幕顺序)。
// ssid/ip/code 允许为空串:空则该行退化成不含该信息的提示,不编造内容。
void np_prompt_lan(np_prompt_t *out, const char *ssid, const char *ip,
                   const char *code);

// 按新读到的 ip 重算四行,和 io 里正显示的比较:有任何一行不同就写回 io 并返回
// true(该重画了),完全相同则不动 io 返回 false。配对码页上的地址是 DHCP 的快照,
// 断线自愈换租约后它会失效,所以调用方要每秒复查(np-0.1.18 实测:屏幕上还是旧
// 地址,点开是无人在的旧 IP)。
bool np_prompt_lan_update(np_prompt_t *io, const char *ssid, const char *ip,
                          const char *code);
