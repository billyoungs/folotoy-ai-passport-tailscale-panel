// main/np_display_cfg.h —— 显示设置(亮度/熄屏间隔/熄屏判定)的纯逻辑。
// 真正的 bsp_display_backlight 调用在 app 层,时间由调用方注入(毫秒)以便主机测试。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// 亮度合法区间:1..100(0 留给熄屏逻辑专用,不许用户把屏幕调到全灭)。
#define NP_DISPLAY_BRIGHTNESS_MIN 1u
#define NP_DISPLAY_BRIGHTNESS_MAX 100u

// 熄屏间隔档位(分钟),0 = 从不。界面与校验共用这一份契约。
#define NP_DISPLAY_OFF_OPTIONS_COUNT 7u
extern const uint8_t np_display_off_options[NP_DISPLAY_OFF_OPTIONS_COUNT];

bool np_display_cfg_valid_brightness(uint8_t v);
uint8_t np_display_cfg_clamp_brightness(int v);
bool np_display_cfg_valid_off_minutes(uint8_t minutes);

// 把分钟档位译成 "从不/1 分钟/…/1 小时/2 小时"。buf 始终 NUL 结尾。
void np_display_cfg_off_label(uint8_t minutes, char *buf, size_t cap);

// 到点判定:off_minutes==0(从不)恒 false;now<last 视为还没到点;
// now-last >= off*60000 才算到点。全程 int64,无乘法溢出。
bool np_display_cfg_should_sleep(int64_t now_ms, int64_t last_activity_ms,
                                 uint8_t off_minutes);