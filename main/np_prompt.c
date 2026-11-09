// main/np_prompt.c —— 提示页文案组装:固定行 + 动态值,长度按屏幕容量收敛。
#include "np_prompt.h"

#include <stdbool.h>
#include <string.h>

// 逐字节复制;若刚好切在一个多字节序列中间,把该序列的首字节也退掉,
// 宁可少显示一个字符,也不给 LVGL 留半个字符(渲染成乱码方块)。
static void copy_fit(char *dst, size_t cap, const char *src)
{
    if (dst == NULL || cap == 0u) return;
    dst[0] = '\0';
    if (src == NULL) return;

    size_t i = 0u;
    while (src[i] != '\0' && i + 1u < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';

    size_t lead = i;
    size_t tail = 0u;
    while (lead > 0u && ((unsigned char)dst[lead - 1u] & 0xC0u) == 0x80u) {
        lead--;
        tail++;
    }
    if (lead > 0u && (unsigned char)dst[lead - 1u] >= 0xC0u) {
        const unsigned char head = (unsigned char)dst[lead - 1u];
        const size_t want = head >= 0xF0u ? 4u : head >= 0xE0u ? 3u : 2u;
        if (tail + 1u < want) dst[lead - 1u] = '\0';
    }
}

// 前缀 + 动态值 + 后缀,总长不超过 NP_PROMPT_LINE(含终止 NUL)。
static void set_line(char *dst, const char *prefix, const char *value,
                     const char *suffix)
{
    const size_t cap = NP_PROMPT_LINE;
    copy_fit(dst, cap, prefix);
    size_t n = strlen(dst);
    if (n + 1u >= cap) return;
    if (value != NULL && value[0] != '\0') {
        copy_fit(dst + n, cap - n, value);
        n += strlen(dst + n);
    }
    if (suffix != NULL && suffix[0] != '\0' && n < cap) {
        copy_fit(dst + n, cap - n, suffix);
    }
}

// 没地址时 ip_str 交的是空串;这里再把 "0.0.0.0" 也认成没有,双重保险:
// 设备一旦把全零地址印出来,用户看到的就是一次"成功"的死链接。
static bool has_addr(const char *ip)
{
    return ip != NULL && ip[0] != '\0' && strcmp(ip, "0.0.0.0") != 0;
}

void np_prompt_lan(np_prompt_t *out, const char *ssid, const char *ip,
                   const char *code)
{
    if (out == NULL) return;

    if (ssid != NULL && ssid[0] != '\0') {
        set_line(out->line[0], "Wi-Fi 已连:", ssid, "");
    } else {
        set_line(out->line[0], "Wi-Fi 已连接", NULL, NULL);
    }
    // 这一页是给"站在设备前、手里没有浏览器"的人看的:把"换一台设备"写在
    // 第一屏,IP 直连不再提 mDNS 名(解析不了 .local 的系统太多,网页头部另有提示)。
    copy_fit(out->line[1], NP_PROMPT_LINE, "用其他设备录入 Tailscale 凭据");
    if (has_addr(ip)) {
        set_line(out->line[2], "打开 http://", ip, "");
    } else {
        set_line(out->line[2], "未取得 IP,长按确定重试", NULL, NULL);
    }
    if (code != NULL && code[0] != '\0') {
        set_line(out->line[3], "配对码 ", code, " 长按中止");
    } else {
        set_line(out->line[3], "长按确定 = 中止配置", NULL, NULL);
    }
}

bool np_prompt_lan_update(np_prompt_t *io, const char *ssid, const char *ip,
                          const char *code)
{
    if (io == NULL) return false;
    np_prompt_t next;
    np_prompt_lan(&next, ssid, ip, code);
    bool changed = false;
    for (int i = 0; i < NP_PROMPT_LINES; i++) {
        if (strcmp(io->line[i], next.line[i]) != 0) changed = true;
    }
    if (changed) *io = next;
    return changed;
}
