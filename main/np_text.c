// main/np_text.c —— UTF-8 解码 + 字形清表过滤实现。纯逻辑,无 ESP/LVGL 依赖。
#include "np_text.h"
#include "np_inventory.h"
#include <string.h>

int np_utf8_next(const char *s, size_t len, size_t *consumed, uint32_t *cp)
{
    if (len == 0) {
        *consumed = 0;
        *cp = 0;
        return 0;
    }
    const unsigned char c0 = (unsigned char)s[0];
    size_t need;
    uint32_t acc;
    if (c0 < 0x80u) { *consumed = 1; *cp = c0; return 1; }
    else if ((c0 & 0xE0u) == 0xC0u) { need = 2; acc = c0 & 0x1Fu; }
    else if ((c0 & 0xF0u) == 0xE0u) { need = 3; acc = c0 & 0x0Fu; }
    else if ((c0 & 0xF8u) == 0xF0u) { need = 4; acc = c0 & 0x07u; }
    else { *consumed = 1; *cp = 0; return -1; }          // 非法首字节
    if (len < need) { *consumed = 1; *cp = 0; return -1; } // 截断序列
    for (size_t i = 1; i < need; i++) {
        const unsigned char ci = (unsigned char)s[i];
        if ((ci & 0xC0u) != 0x80u) { *consumed = 1; *cp = 0; return -1; }
        acc = (acc << 6) | (ci & 0x3Fu);
    }
    // 拒绝过长编码与代理区/越界值:它们不是合法交换格式。
    if ((need == 2 && acc < 0x80u) || (need == 3 && acc < 0x800u) ||
        (need == 4 && acc < 0x10000u) || (acc >= 0xD800u && acc <= 0xDFFFu) ||
        acc > 0x10FFFFu) {
        *consumed = 1; *cp = 0; return -1;
    }
    *consumed = need;
    *cp = acc;
    return 1;
}

// 清表在生成时按升序输出,可二分;不满足前提时 extract_chars.py 会失败。
bool np_text_supported(uint32_t cp)
{
    if (cp >= 0x20u && cp <= 0x7Eu) return true;
    size_t lo = 0, hi = np_inventory_count;
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2u;
        if (np_inventory[mid] == cp) return true;
        if (np_inventory[mid] < cp) lo = mid + 1u; else hi = mid;
    }
    return false;
}

int np_text_sanitize(char *dst, size_t cap, const char *src)
{
    if (cap == 0u || dst == NULL || src == NULL) return -1;
    size_t out = 0u;
    size_t used = 0u;
    int replaced = 0;
    const size_t srclen = strlen(src);
    while (used < srclen) {
        size_t adv = 0u;
        uint32_t cp = 0u;
        const int ok = np_utf8_next(src + used, srclen - used, &adv, &cp);
        const bool keep = (ok > 0) && np_text_supported(cp);
        char ch = '?';
        size_t n = 1u;
        if (keep && cp >= 0x20u && cp <= 0x7Eu) { ch = (char)cp; }
        else if (keep) {
            // 重编码清表内非 ASCII 码点(逐字节构造,避免依赖 locale)。
            n = (cp < 0x800u) ? 2u : (cp < 0x10000u) ? 3u : 4u;
            if (out + n >= cap) break;
            if (n == 2u) {
                dst[out++] = (char)(0xC0u | (cp >> 6));
                dst[out++] = (char)(0x80u | (cp & 0x3Fu));
            } else if (n == 3u) {
                dst[out++] = (char)(0xE0u | (cp >> 12));
                dst[out++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
                dst[out++] = (char)(0x80u | (cp & 0x3Fu));
            } else {
                dst[out++] = (char)(0xF0u | (cp >> 18));
                dst[out++] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
                dst[out++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
                dst[out++] = (char)(0x80u | (cp & 0x3Fu));
            }
            used += adv;
            continue;
        } else {
            replaced++;
        }
        if (out + n >= cap) break;
        dst[out++] = ch;
        used += (adv == 0u) ? 1u : adv;
    }
    dst[out] = '\0';
    return replaced;
}
