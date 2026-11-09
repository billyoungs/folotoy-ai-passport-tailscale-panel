// tests/test_np_time.c —— RFC3339 解析与相对时间格式化测试。
#include <assert.h>
#include <string.h>
#include "np_time.h"

int main(void)
{
    int64_t t;

    // 标准 Z 时区
    assert(np_time_parse_rfc3339("2026-11-02T22:30:59Z", &t));
    assert(t == 1793658659);
    // 带毫秒与 +08:00 偏移:与 2026-11-01T22:30:59Z 同一时刻
    assert(np_time_parse_rfc3339("2026-11-02T06:30:59.123456+08:00", &t));
    assert(t == 1793658659 - 86400);
    assert(np_time_parse_rfc3339("2026-11-01T22:30:59Z", &t));
    assert(t == 1793658659 - 86400);
    // 非法输入
    assert(!np_time_parse_rfc3339("2026-11-02 22:30:59", &t)); // 缺 T
    assert(!np_time_parse_rfc3339("2026-11-02T22:30:59", &t)); // 缺时区
    assert(!np_time_parse_rfc3339("bogus", &t));
    assert(!np_time_parse_rfc3339(NULL, &t));
    assert(!np_time_parse_rfc3339("2026-13-01T00:00:00Z", &t)); // 非法月份
    // 控制面拿 Go 的零值时间表示"还连着",这类串是合法 RFC3339,解析必须成功;
    // 它算不算断线时刻由 np_seen_disconnect_at 决定(见 test_np_model.c)。
    assert(np_time_parse_rfc3339("0001-01-01T00:00:00Z", &t));
    assert(t == -62135596800LL);
    assert(np_time_parse_rfc3339("1970-01-01T00:00:00Z", &t));
    assert(t == 0);

    // 相对时间
    char buf[32];
    np_time_format_ago(buf, sizeof(buf), 1000, 1000);
    assert(strcmp(buf, "刚刚") == 0);
    np_time_format_ago(buf, sizeof(buf), 1000 + 5 * 60, 1000);
    assert(strcmp(buf, "5分钟前") == 0);
    np_time_format_ago(buf, sizeof(buf), 1000 + 3 * 3600, 1000);
    assert(strcmp(buf, "3小时前") == 0);
    np_time_format_ago(buf, sizeof(buf), 1000 + 10 * 86400, 1000);
    assert(strcmp(buf, "10天前") == 0);
    np_time_format_ago(buf, sizeof(buf), 1000, 1000 + 1);
    assert(strcmp(buf, "未来") == 0);

    // MDHM 展示(UTC 与时区偏移)
    np_time_format_mdhm(buf, sizeof(buf), 1793658659, 0);
    assert(strcmp(buf, "11-02 22:30") == 0);
    np_time_format_mdhm(buf, sizeof(buf), 1793658659, 8 * 3600);
    assert(strcmp(buf, "11-03 06:30") == 0);

    // 状态栏把日期和时间拆成两格:左边 M/D 不补零,中间 HH:MM 补零。
    np_time_format_md(buf, sizeof(buf), 1793658659, 0);
    assert(strcmp(buf, "11/2") == 0);
    np_time_format_hm(buf, sizeof(buf), 1793658659, 0);
    assert(strcmp(buf, "22:30") == 0);
    // 偏移把日期推过午夜:小时补零、月份不补零。
    np_time_format_md(buf, sizeof(buf), 1793658659, 8 * 3600);
    assert(strcmp(buf, "11/3") == 0);
    np_time_format_hm(buf, sizeof(buf), 1793658659, 8 * 3600);
    assert(strcmp(buf, "06:30") == 0);
    // 单位数月日 + 午夜/年末边界,确认拆出来的两格讲的是同一时刻。
    np_time_parse_rfc3339("2027-01-01T00:00:00Z", &t);
    np_time_format_md(buf, sizeof(buf), t, 0);
    assert(strcmp(buf, "1/1") == 0);
    np_time_format_hm(buf, sizeof(buf), t, 0);
    assert(strcmp(buf, "00:00") == 0);
    // Go 零值那种大负数也不能算出个不存在的月份。
    np_time_format_md(buf, sizeof(buf), -62135596800LL, 0);
    assert(strcmp(buf, "1/1") == 0);
    np_time_format_hm(buf, sizeof(buf), -62135596800LL, 0);
    assert(strcmp(buf, "00:00") == 0);
    // 缓冲太小或空指针不许越界(截断即可,状态栏两格都留了余量)。
    np_time_format_md(buf, 0, 1793658659, 0);
    np_time_format_hm(NULL, 32, 1793658659, 0);

    // 剩余天数:过期为负
    assert(np_time_days_until(1000 + 5 * 86400 + 1, 1000) == 5);
    assert(np_time_days_until(1000, 1000) == 0);
    assert(np_time_days_until(1000 - 1, 1000) == -1);
    return 0;
}
