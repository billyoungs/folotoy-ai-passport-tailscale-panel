// main/netpanel_app.h —— 应用编排:状态机 + 按键/UI 任务 + 轮询任务。
#pragma once

#include "bsp_button.h"
#include "esp_err.h"
#include <stdbool.h>

// BSP 显示/LVGL 已由 main.c 初始化后调用;创建任务并进入首屏。
esp_err_t np_app_start(void);

// 按键回调入口(由 main.c 的 on_key 在守卫撤防后调用)。
// 返回 false 表示事件队列尚未创建或已满,事件被丢弃。
bool np_app_key_event(bsp_btn_t btn, bsp_btn_ev_t ev);
