// main/np_json.c —— JSON 片段追加。
#include "np_json.h"

#include <stdio.h>
#include <string.h>

bool np_json_lit(char *out, size_t cap, size_t *used, const char *lit)
{
    if (out == NULL || used == NULL || lit == NULL || *used >= cap) return false;
    const size_t n = strlen(lit);
    if (*used + n >= cap) return false;
    memcpy(out + *used, lit, n);
    *used += n;
    out[*used] = '\0';
    return true;
}

bool np_json_int(char *out, size_t cap, size_t *used, long long value)
{
    char num[24];
    const int n = snprintf(num, sizeof(num), "%lld", value);
    if (n < 0 || (size_t)n >= sizeof(num)) return false;
    return np_json_lit(out, cap, used, num);
}

bool np_json_str(char *out, size_t cap, size_t *used, const char *val)
{
    if (out == NULL || used == NULL || val == NULL || *used >= cap) return false;

    size_t u = *used;
    for (const unsigned char *s = (const unsigned char *)val; *s; s++) {
        size_t need = 1u;
        if (*s == '"' || *s == '\\') need = 2u;
        else if (*s < 0x20u) need = 6u;
        if (u + need >= cap) return false;   // 留一位给 NUL

        if (*s == '"' || *s == '\\') {
            out[u++] = '\\';
            out[u++] = (char)*s;
        } else if (*s < 0x20u) {
            static const char hex[] = "0123456789abcdef";
            memcpy(out + u, "\\u00", 4);
            u += 4;
            out[u++] = hex[*s >> 4];
            out[u++] = hex[*s & 0xFu];
        } else {
            out[u++] = (char)*s;
        }
    }
    out[u] = '\0';
    *used = u;
    return true;
}
