// main/np_json.h —— 手写 JSON 片段追加(纯逻辑)。
// 配网响应只有几百字节,不值得为它拉进 cJSON;但用户可控文本必须安全嵌入。
// 约定:三个函数都往 out 的 *used 位置续写,NUL 结尾;任何一次失败都代表
// 整条响应作废,调用方必须改发错误 JSON,而不是把半截缓冲发出去。
#pragma once
#include <stdbool.h>
#include <stddef.h>

// 追加固定片段(键名、括号、数字以外的字面量)。
bool np_json_lit(char *out, size_t cap, size_t *used, const char *lit);

// 追加十进制整数。
bool np_json_int(char *out, size_t cap, size_t *used, long long value);

// 追加字符串值的内容(不含外层引号)。双引号与反斜杠加转义,控制字符转成
// u00xx 十六进制转义,其余字节原样写出(UTF-8 合法)。
bool np_json_str(char *out, size_t cap, size_t *used, const char *val);
