// main/main.c —— 网络面板(Network Engineer Panel)应用入口。
// 职责只做 BSP 引导:显示是硬依赖,失败即退出;音频/蓝牙不在本应用范围。
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_pins.h"      // 错误日志里要打印 BSP_LCD_* 引脚号
#include "key_guard.h"
#include "netpanel_app.h"
#include "app_cfg.h"
#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include <stdbool.h>

static const char *TAG = "main";

// 唤醒键在按键组件开始观测前就松手时不会有 release 事件,deadline 只是卡键兜底。
#define KEY_GUARD_MAX_US (10LL * 1000 * 1000)

static volatile bool s_input_ready;
static key_guard_t s_key_guard;

// 按键回调跑在共享 esp_timer 任务上,只做守卫判断和转发。
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;
    const bool was_armed = s_key_guard.armed;
    if (key_guard_consume(&s_key_guard, ev == BSP_BTN_RELEASE, esp_timer_get_time())) {
        if (was_armed && !s_key_guard.armed) {
            ESP_LOGI(TAG, "唤醒键已松手,按键恢复响应");
        }
        return;
    }
    if (!s_input_ready || !np_app_key_event(btn, ev)) {
        ESP_LOGD(TAG, "按键事件被丢弃 btn=%d ev=%d", (int)btn, (int)ev);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "网络面板启动 %s(FoloToy AI Passport)",
             esp_app_get_description()->version);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 面板不睡深睡,但用 GPIO 唤醒开机(按下电源键上电)时那颗键仍按着。
    const uint32_t wake_causes = esp_sleep_get_wakeup_causes();
    if (wake_causes & (1u << ESP_SLEEP_WAKEUP_GPIO)) {
        key_guard_arm(&s_key_guard, esp_timer_get_time(), KEY_GUARD_MAX_US);
        ESP_LOGI(TAG, "按键唤醒:先屏蔽按键动作,直到那颗键松手");
    }

    bsp_i2c_init();

    // 屏幕是本应用的唯一载体,失败就没有界面可言,打清楚日志后退出。
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "显示/LVGL 初始化失败,应用无法继续。"
                      "检查 SPI 接线(MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    if (bsp_battery_init() != ESP_OK) {
        ESP_LOGW(TAG, "电量计不可用,界面电量显示为 --");
    }
    (void)app_cfg_load();

    err = np_app_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "应用启动失败: %s", esp_err_to_name(err));
        return;
    }

    err = bsp_button_init(on_key, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "按键初始化失败: %s,界面只能看不能操作", esp_err_to_name(err));
        return;
    }
    if (s_key_guard.armed) {
        // 按键组件启动前就松手的话不会有 release 事件,按采样电平提前撤防。
        const bool released = bsp_button_read_mv() >= BSP_BTN_MV_RELEASED_MIN;
        key_guard_sync_released(&s_key_guard, released);
        if (!s_key_guard.armed) {
            ESP_LOGI(TAG, "唤醒键在按键驱动启动前已松手,提前撤防");
        }
    }
    s_input_ready = true;
}
