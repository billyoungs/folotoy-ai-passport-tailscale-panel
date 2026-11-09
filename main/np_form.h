// main/np_form.h —— application/x-www-form-urlencoded 解析与配对码校验(纯逻辑)。
// 配网页 POST 体的唯一入口;凭据字段解析后即拷走,不记录原始 body。
#pragma once
#include <stdbool.h>
#include <stddef.h>

// 从 "a=1&b=%20x" 形式的 body 中提取键值,做 %-解码并把 '+' 还原为空格。
// out 容量 out_cap(含 NUL);超长时截断到 out_cap-1 并返回 false(仍写出部分内容)。
// 键不存在返回 false 且 out[0]='\0'。
bool np_form_get(const char *body, const char *key, char *out, size_t out_cap);

// 配对码:恰好 6 位十进制。
bool np_pairing_code_valid(const char *code);
