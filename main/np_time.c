// main/np_time.c —— 无时区数据库的定长格式解析;适配 Tailscale API 的 RFC3339 输出。
#include "np_time.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Howard Hinnant days_from_civil:公历日期 → 距 1970-01-01 天数(闰年规则内建)。
static int64_t days_from_civil(int64_t y, unsigned m, unsigned d)
{
    y -= m <= 2u;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153u * (m + (m > 2u ? -3u : 9u)) + 2u) / 5u + d - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

bool np_time_parse_rfc3339(const char *s, int64_t *epoch_out)
{
    if (s == NULL || epoch_out == NULL) return false;
    int y, mo, d, h, mi, sec;
    if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &y, &mo, &d, &h, &mi, &sec) != 6)
        return false;
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || sec > 60)
        return false;
    const char *p = strchr(s, 'T');
    if (p == NULL) return false;
    p += 1 + 8; // 跳过 "HH:MM:SS"(8 字符)
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') p++;
    }
    int off = 0;
    if (*p == 'Z' || *p == 'z') {
        off = 0;
    } else if (*p == '+' || *p == '-') {
        int oh = 0, om = 0;
        if (sscanf(p + 1, "%2d:%2d", &oh, &om) < 1) return false;
        off = oh * 3600 + om * 60;
        if (*p == '-') off = -off;
    } else {
        return false; // 必须带时区标记
    }
    *epoch_out = days_from_civil(y, (unsigned)mo, (unsigned)d) * 86400ll
               + h * 3600ll + mi * 60ll + sec - off;
    return true;
}

void np_time_format_ago(char *buf, size_t cap, int64_t now, int64_t past)
{
    if (buf == NULL || cap == 0u) return;
    const int64_t delta = now - past;
    if (delta < 0) { snprintf(buf, cap, "未来"); return; }
    if (delta < 60) { snprintf(buf, cap, "刚刚"); return; }
    if (delta < 3600) { snprintf(buf, cap, "%u分钟前", (unsigned)(delta / 60)); return; }
    if (delta < 86400) { snprintf(buf, cap, "%u小时前", (unsigned)(delta / 3600)); return; }
    snprintf(buf, cap, "%u天前", (unsigned)(delta / 86400));
}

// 本地秒(= epoch + 偏移)拆成月、日、一天内秒数。days → civil 与
// days_from_civil 互逆;1970 之前的负值要先向下取整,否则天数会多算一天
// (控制面用 Go 零值时间表示"还连着",那个 epoch 就是大负数)。
static void civil_of(int64_t local, unsigned *mon, unsigned *day, unsigned *sod)
{
    int64_t days = local / 86400;
    if (local % 86400 < 0) days--;
    int64_t sec_of_day = local - days * 86400;

    days += 719468;
    const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const unsigned doe = (unsigned)(days - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5u * doy + 2u) / 153u;
    *mon = mp + (mp < 10u ? 3u : -9u);
    *day = doy - (153u * mp + 2u) / 5u + 1u;
    *sod = (unsigned)sec_of_day;
}

void np_time_format_mdhm(char *buf, size_t cap, int64_t epoch, int32_t offset_s)
{
    if (buf == NULL || cap == 0u) return;
    unsigned m, d, sod;
    civil_of(epoch + offset_s, &m, &d, &sod);
    snprintf(buf, cap, "%02u-%02u %02u:%02u", m % 100u, d, (sod / 3600u) % 24u,
             (sod % 3600u) / 60u);
}

void np_time_format_md(char *buf, size_t cap, int64_t epoch, int32_t offset_s)
{
    if (buf == NULL || cap == 0u) return;
    unsigned m, d, sod;
    civil_of(epoch + offset_s, &m, &d, &sod);
    snprintf(buf, cap, "%u/%u", m, d);
}

void np_time_format_hm(char *buf, size_t cap, int64_t epoch, int32_t offset_s)
{
    if (buf == NULL || cap == 0u) return;
    unsigned m, d, sod;
    civil_of(epoch + offset_s, &m, &d, &sod);
    snprintf(buf, cap, "%02u:%02u", (sod / 3600u) % 24u, (sod % 3600u) / 60u);
}

int np_time_days_until(int64_t epoch, int64_t now)
{
    const int64_t delta = epoch - now;
    if (delta >= 0) return (int)(delta / 86400ll);
    return -(int)((-delta + 86399ll) / 86400ll);
}
