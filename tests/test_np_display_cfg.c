// tests/test_np_display_cfg.c —— 显示设置(亮度/熄屏间隔/熄屏判定)的纯逻辑测试。
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "np_display_cfg.h"

int main(void)
{
    // ---- 亮度合法性:0 是熄屏专用值,不许用户存进配置
    assert(!np_display_cfg_valid_brightness(0u));
    assert(np_display_cfg_valid_brightness(1u));
    assert(np_display_cfg_valid_brightness(50u));
    assert(np_display_cfg_valid_brightness(100u));
    assert(!np_display_cfg_valid_brightness(101u));

    // ---- 亮度夹紧:负中间值(步进越界)夹到下限,超上限夹到上限
    assert(np_display_cfg_clamp_brightness(-5) == 1u);
    assert(np_display_cfg_clamp_brightness(0) == 1u);
    assert(np_display_cfg_clamp_brightness(1) == 1u);
    assert(np_display_cfg_clamp_brightness(50) == 50u);
    assert(np_display_cfg_clamp_brightness(100) == 100u);
    assert(np_display_cfg_clamp_brightness(105) == 100u);

    // ---- 熄屏间隔档位:只有约定档位合法
    for (size_t i = 0; i < NP_DISPLAY_OFF_OPTIONS_COUNT; i++) {
        assert(np_display_cfg_valid_off_minutes(np_display_off_options[i]));
    }
    assert(!np_display_cfg_valid_off_minutes(2u));
    assert(!np_display_cfg_valid_off_minutes(15u));
    assert(!np_display_cfg_valid_off_minutes(45u));
    assert(!np_display_cfg_valid_off_minutes(61u));
    assert(!np_display_cfg_valid_off_minutes(0xFFu));

    // ---- 档位文案:从不 / 分钟 / 小时
    char buf[24];
    np_display_cfg_off_label(0u, buf, sizeof(buf));
    assert(strcmp(buf, "从不") == 0);
    np_display_cfg_off_label(1u, buf, sizeof(buf));
    assert(strcmp(buf, "1 分钟") == 0);
    np_display_cfg_off_label(30u, buf, sizeof(buf));
    assert(strcmp(buf, "30 分钟") == 0);
    np_display_cfg_off_label(60u, buf, sizeof(buf));
    assert(strcmp(buf, "1 小时") == 0);
    np_display_cfg_off_label(120u, buf, sizeof(buf));
    assert(strcmp(buf, "2 小时") == 0);
    // 空缓冲/零容量不能写穿
    np_display_cfg_off_label(10u, NULL, 0);
    np_display_cfg_off_label(10u, buf, 1);
    assert(buf[0] == '\0');

    // ---- 到点判定:从不恒 false
    assert(!np_display_cfg_should_sleep(1000000, 0, 0u));
    // 未到点 / 恰好到点 / 差一秒
    assert(!np_display_cfg_should_sleep(1000, 0, 1u));          // 1 分钟未到
    assert(!np_display_cfg_should_sleep(59999, 0, 1u));
    assert(np_display_cfg_should_sleep(60000, 0, 1u));          // 恰好 1 分钟
    assert(np_display_cfg_should_sleep(60001, 0, 1u));
    // now < last(异常时钟回拨):视为未到点,不能触发
    assert(!np_display_cfg_should_sleep(0, 1000, 5u));
    // 大时间跨度不溢出:长时间运行后 last 很大,差值仍精确
    const int64_t big = INT64_C(1) << 40;
    assert(np_display_cfg_should_sleep(big + 120 * 60000, big, 120u));
    assert(!np_display_cfg_should_sleep(big + 120 * 60000 - 1, big, 120u));

    printf("np_display_cfg: all tests passed\n");
    return 0;
}