// main/np_text.h —— UTF-8 遍历与动态文本按字形清表过滤。
// 契约:动态内容(主机名/属主/SSID 等)只允许 ASCII 可打印字符与字体清表内码点;
// 其余按码点替换为 '?'。禁止按字节截断,禁止静默丢字符。
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// 解析一个 UTF-8 序列。返回序列字节数(1..4),*cp 输出码点。
// 非法/截断序列返回 1 并令 *cp=0(消费 1 字节,调用方以 '?' 呈现)。
int np_utf8_next(const char *s, size_t len, size_t *consumed, uint32_t *cp);

// 码点是否可呈现:ASCII 可打印(0x20..0x7E)或位于字体清表。
bool np_text_supported(uint32_t cp);

// 将 src 过滤到 dst(cap 字节,含终止 NUL)。返回替换的码点个数。
// dst 始终 NUL 结尾;cap=0 时不动 dst 并返回 -1。
int np_text_sanitize(char *dst, size_t cap, const char *src);
