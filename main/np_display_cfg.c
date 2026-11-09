// main/np_display_cfg.c —— 显示设置的纯逻辑:校验与档位都在这里,app 只做搬运。
#include "np_display_cfg.h"

#include <stdio.h>

const uint8_t np_display_off_options[NP_DISPLAY_OFF_OPTIONS_COUNT] = {
    0u, 1u, 5u, 10u, 30u, 60u, 120u,
};

bool np_display_cfg_valid_brightness(uint8_t v)
{
    return v >= NP_DISPLAY_BRIGHTNESS_MIN && v <= NP_DISPLAY_BRIGHTNESS_MAX;
}

uint8_t np_display_cfg_clamp_brightness(int v)
{
    if (v < (int)NP_DISPLAY_BRIGHTNESS_MIN) return NP_DISPLAY_BRIGHTNESS_MIN;
    if (v > (int)NP_DISPLAY_BRIGHTNESS_MAX) return NP_DISPLAY_BRIGHTNESS_MAX;
    return (uint8_t)v;
}

bool np_display_cfg_valid_off_minutes(uint8_t minutes)
{
    for (size_t i = 0; i < NP_DISPLAY_OFF_OPTIONS_COUNT; i++) {
        if (np_display_off_options[i] == minutes) return true;
    }
    return false;
}

void np_display_cfg_off_label(uint8_t minutes, char *buf, size_t cap)
{
    if (buf == NULL || cap == 0u) return;
    if (minutes == 0u) {
        snprintf(buf, cap, "从不");
    } else if (minutes % 60u == 0u) {
        snprintf(buf, cap, "%u 小时", (unsigned)(minutes / 60u));
    } else {
        snprintf(buf, cap, "%u 分钟", (unsigned)minutes);
    }
}

bool np_display_cfg_should_sleep(int64_t now_ms, int64_t last_activity_ms,
                                 uint8_t off_minutes)
{
    if (off_minutes == 0u) return false;
    if (now_ms < last_activity_ms) return false;
    return (now_ms - last_activity_ms) >= (int64_t)off_minutes * 60000;
}