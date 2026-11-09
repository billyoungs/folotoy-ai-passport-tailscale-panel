// main/np_jstream.c —— 字符级 JSON 切片状态机。
#include "np_jstream.h"

#include <string.h>

static bool is_space(char b)
{
    return b == ' ' || b == '\t' || b == '\n' || b == '\r';
}

static void key_push(np_jstream_t *s, char b)
{
    if (s->key_len + 1u < NP_JS_KEY_CAP) {
        s->key[s->key_len++] = b;
        s->key[s->key_len] = '\0';
    } else {
        s->key_trunc = true;   // 超长键名不可能是我们要找的那个
    }
}

static bool key_is_devices(const np_jstream_t *s)
{
    return !s->key_trunc && s->key_len == 7u &&
           memcmp(s->key, "devices", 7) == 0;
}

void np_jstream_init(np_jstream_t *s, char *obj, size_t cap)
{
    if (s == NULL) return;
    s->obj = obj;
    s->cap = cap;
    s->used = 0;
    s->phase = NP_JS_PRE;
    s->depth = 0;
    s->in_str = false;
    s->esc = false;
    s->expect_colon = false;
    s->key_trunc = false;
    s->overflow = false;
    s->stop = false;
    s->key_len = 0;
    s->key[0] = '\0';
    s->total = 0;
}

// 根对象里找 "devices": 后面的数组。键名与字符串值在这里长得一样,靠"闭合的
// 字符串后面紧跟 ':'"区分,再靠 depth==1 排除嵌套对象里同名的键。
static void pre_byte(np_jstream_t *s, char b)
{
    if (s->in_str) {
        if (s->esc) { s->esc = false; return; }
        if (b == '\\') { s->esc = true; return; }
        if (b != '"') { key_push(s, b); return; }
        s->in_str = false;
        s->expect_colon = true;
        return;
    }
    if (b == '"') {
        s->key_len = 0;
        s->key[0] = '\0';
        s->key_trunc = false;
        s->in_str = true;
        return;
    }
    if (s->expect_colon) {
        if (is_space(b)) return;
        s->expect_colon = false;
        if (b == ':') {
            if (s->depth == 1 && key_is_devices(s)) s->phase = NP_JS_ARR;
            return;
        }
        // 不是 ':' 就说明刚闭合的那段是值不是键,而这一字节本身(逗号、括号)
        // 仍然要按容器处理,否则深度会算漏。
    }
    if (b == '{' || b == '[') { s->depth++; return; }
    if (b == '}' || b == ']') {
        if (s->depth == 0) { s->phase = NP_JS_BAD; return; }
        s->depth--;
        // 根对象合上了还没见到 devices:这份响应里没有列表可读。
        if (s->depth == 0) s->phase = NP_JS_BAD;
    }
}

static void arr_byte(np_jstream_t *s, char b)
{
    if (is_space(b)) return;
    if (b == '[') { s->phase = NP_JS_ITEM; return; }
    // devices 不是数组(空 tailnet 给 null 还是给 [] 都不固定,后者走 ITEM):
    // 不是列表就没法逐项读,别再猜。
    s->phase = NP_JS_BAD;
}

static void item_byte(np_jstream_t *s, char b)
{
    if (is_space(b) || b == ',') return;
    if (b == '{') {
        if (s->cap < 2u) { s->phase = NP_JS_BAD; return; }  // 调用方给错缓冲
        s->obj[0] = '{';
        s->used = 1;
        s->depth = 1;
        s->in_str = false;
        s->esc = false;
        s->overflow = false;
        s->phase = NP_JS_OBJ;
        return;
    }
    if (b == ']') { s->phase = NP_JS_DONE; return; }
    s->phase = NP_JS_BAD;   // 数组元素不是对象
}

static void obj_byte(np_jstream_t *s, char b, np_js_fn cb, void *user)
{
    // 先按这一字节推进层级,再决定收不收进缓冲:溢出的时候状态机也必须走完,
    // 否则后面每一台都会错位。
    if (s->in_str) {
        if (s->esc) s->esc = false;
        else if (b == '\\') s->esc = true;
        else if (b == '"') s->in_str = false;
    } else if (b == '"') {
        s->in_str = true;
    } else if (b == '{') {
        s->depth++;
    } else if (b == '}') {
        s->depth--;
    }

    if (!s->overflow && s->used + 1u < s->cap) {
        s->obj[s->used++] = b;
    } else {
        s->overflow = true;
    }
    if (s->depth > 0) return;

    s->total++;
    s->obj[s->used] = '\0';
    if (!s->stop && cb != NULL) {
        cb(user, s->overflow ? NULL : s->obj, !s->overflow);
    }
    s->phase = NP_JS_ITEM;
}

void np_jstream_feed(np_jstream_t *s, const char *data, size_t len,
                     np_js_fn cb, void *user)
{
    if (s == NULL || s->obj == NULL || s->cap == 0u) return;
    for (size_t i = 0; i < len; i++) {
        if (s->phase == NP_JS_DONE || s->phase == NP_JS_BAD) return;
        const char b = data[i];
        switch (s->phase) {
        case NP_JS_PRE:  pre_byte(s, b); break;
        case NP_JS_ARR:  arr_byte(s, b); break;
        case NP_JS_ITEM: item_byte(s, b); break;
        case NP_JS_OBJ:  obj_byte(s, b, cb, user); break;
        default: return;
        }
    }
}

void np_jstream_stop(np_jstream_t *s)
{
    if (s != NULL) s->stop = true;
}

bool np_jstream_finished(const np_jstream_t *s)
{
    return s != NULL && (s->phase == NP_JS_DONE || s->phase == NP_JS_BAD);
}

bool np_jstream_ok(const np_jstream_t *s)
{
    return s != NULL && s->phase == NP_JS_DONE;
}

uint32_t np_jstream_total(const np_jstream_t *s)
{
    return s ? s->total : 0u;
}
