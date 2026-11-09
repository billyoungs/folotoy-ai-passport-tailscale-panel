// main/np_form.c —— 无第三方依赖的表单解析。主机可测。
#include "np_form.h"
#include <string.h>

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// 比较 body 中 [s,e) 区间与 key 是否完全相等。
static bool key_eq(const char *s, const char *e, const char *key)
{
    const size_t klen = strlen(key);
    return (size_t)(e - s) == klen && memcmp(s, key, klen) == 0;
}

bool np_form_get(const char *body, const char *key, char *out, size_t out_cap)
{
    if (out_cap == 0u) return false;
    out[0] = '\0';
    if (body == NULL || key == NULL) return false;

    const char *p = body;
    bool found = false;
    bool truncated = false;
    size_t used = 0u;
    while (*p != '\0') {
        const char *amp = strchr(p, '&');
        const char *seg_end = amp ? amp : p + strlen(p);
        const char *eq = memchr(p, '=', (size_t)(seg_end - p));
        if (eq != NULL && key_eq(p, (const char *)eq, key)) {
            found = true;
            const char *v = eq + 1;
            while (v < seg_end) {
                char decoded;
                if (*v == '%' && seg_end - v >= 3 &&
                    hexval(v[1]) >= 0 && hexval(v[2]) >= 0) {
                    decoded = (char)(hexval(v[1]) * 16 + hexval(v[2]));
                    v += 3;
                } else if (*v == '+') {
                    decoded = ' ';
                    v += 1;
                } else {
                    decoded = *v;
                    v += 1;
                }
                if (used + 1u < out_cap) out[used++] = decoded; // 留 NUL 位
                else truncated = true;
            }
            break;
        }
        if (amp == NULL) break;
        p = amp + 1;
    }
    out[used] = '\0';
    return found && !truncated;
}

bool np_pairing_code_valid(const char *code)
{
    if (code == NULL) return false;
    for (int i = 0; i < 6; i++) {
        if (code[i] < '0' || code[i] > '9') return false;
    }
    return code[6] == '\0';
}
