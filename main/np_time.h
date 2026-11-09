// main/np_time.h —— RFC3339 解析与相对时间文案(纯逻辑,可主机测试)。
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// 解析 "YYYY-MM-DDTHH:MM:SS[.fff](Z|±HH:MM)"。成功返回 true 并输出 UTC epoch 秒。
bool np_time_parse_rfc3339(const char *s, int64_t *epoch_out);

// 把 (now - past) 格式化为"刚刚/N分钟前/N小时前/N天前";past>now 输出"未来"。
// 文案全部由清表字符构成。buf 始终 NUL 结尾。
void np_time_format_ago(char *buf, size_t cap, int64_t now, int64_t past);

// epoch → "MM-DD HH:MM"(UTC+offset_s 本地)。详情里要同时看清哪天几点,用这个。
// 简化展示,不处理历法切换。
void np_time_format_mdhm(char *buf, size_t cap, int64_t epoch, int32_t offset_s);

// epoch → "M/D"(不补零,状态栏左边那一格)。同一套历法换算,见 np_time.c。
void np_time_format_md(char *buf, size_t cap, int64_t epoch, int32_t offset_s);

// epoch → "HH:MM"(24 小时,补零;状态栏居中那一格)。
void np_time_format_hm(char *buf, size_t cap, int64_t epoch, int32_t offset_s);

// epoch → 距今剩余天数(向下取整;已过期返回负数)。
int np_time_days_until(int64_t epoch, int64_t now);
