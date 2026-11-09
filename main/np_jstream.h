// main/np_jstream.h —— 从 JSON 字节流里切出数组对象(纯逻辑,无平台依赖)。
// 为什么需要它:这台板子没有 PSRAM,内部堆在 TLS 握手之后只剩 8 KB 上下。设备
// 列表原先是"整包读进一块 4→24 KB 的缓冲,再交给 cJSON 建树",峰值内存跟着
// tailnet 一起长:主机一多,先是 realloc 到 24 KB 直接失败("内存不足,装不下
// 响应"),就算侥幸装下,cJSON 的树还要 2.5~4 倍原始字节,照样起不来。本模块把
// 顶层的 "devices" 数组一台一台切出来,一次只攒一台,切完就交给调用方,峰值内存
// 由"最大的一台"决定而不是"整张表"决定。
//
// 它只切不解释:切出来的片段是合法的单个 JSON 对象文本,字段仍由 cJSON 解析,
// 所以这里不需要认识设备有什么键,也不需要处理嵌套之外的任何语义。字符串里的
// 花括号、转义引号都要避开,因此它自己带一个足够用完这段文本的字符级状态机。
//
// 时间/内存都由调用方给:回调里 cJSON_Parse 一份、解析完立刻 cJSON_Delete。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NP_JS_KEY_CAP 24   // 顶层键名的比对长度,超过就当不匹配

typedef enum {
    NP_JS_PRE = 0,   // 还在根对象里找 "devices" 这个键
    NP_JS_ARR,       // 等它后面的 '['
    NP_JS_ITEM,      // 数组里:等下一个 '{' 或 ']'
    NP_JS_OBJ,       // 正在攒一台设备的对象
    NP_JS_DONE,      // 数组已闭合,后续输入不再看
    NP_JS_BAD,       // 结构对不上(比如 devices 不是数组),同样不再看
} np_js_phase_t;

// 一台设备的原始 JSON 攒好了就回调一次。json 为 NULL / complete 为 false 表示
// 这一台太长,缓冲装不下,片段只是截断的前缀,别拿去解析。
typedef void (*np_js_fn)(void *user, const char *json, bool complete);

typedef struct {
    char *obj;           // 调用方给的缓冲,一次只装一台
    size_t cap;          // 含 NUL
    size_t used;
    np_js_phase_t phase;
    int depth;           // 对象内的花括号深度
    bool in_str;         // 当前在字符串里(引号内的 {} 不算数)
    bool esc;            // 字符串里刚吃到反斜杠
    bool expect_colon;   // 刚闭合一个字符串,下一个非空白若是 ':' 它就是键名
    bool key_trunc;      // 键名超长,不可能是 devices
    bool overflow;       // 当前这台装不下
    bool stop;           // 调用方说别再给了,只数个数
    size_t key_len;
    char key[NP_JS_KEY_CAP];
    uint32_t total;      // 数组里见过的对象个数(含没送进列表的那些)
} np_jstream_t;

void np_jstream_init(np_jstream_t *s, char *obj, size_t cap);

// 喂进 body 的一部分。切出完整一台就回调一次。
void np_jstream_feed(np_jstream_t *s, const char *data, size_t len,
                     np_js_fn cb, void *user);

// 列表已满:后面的只计数,不再回调。
void np_jstream_stop(np_jstream_t *s);

// 数组已闭合或结构不对:调用方可以不再读 body。
bool np_jstream_finished(const np_jstream_t *s);

// body 读完再问:数组是不是干净地闭合了。false = 压根没等到 devices 数组,
// 或形状不对(比如 devices 不是数组)—— 那是解析失败,不是"零台设备"。
bool np_jstream_ok(const np_jstream_t *s);

uint32_t np_jstream_total(const np_jstream_t *s);
