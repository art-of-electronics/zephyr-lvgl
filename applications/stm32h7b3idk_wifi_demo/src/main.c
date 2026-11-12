#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <lvgl.h>

#include <display.h>
#include <backlight.h>
#include <performance.h>
#include <theme.h>

#include <gui_wifi.h>
#include <gui_wifi_scan.h>

LOG_MODULE_REGISTER(app_main, LOG_LEVEL_INF);

#define MAIN_LOOP_PERIOD_MS   (10U)
#define PERF_PRINT_REPORT     (false)
#define ENABLE_BTN_ANIM       (true)


int main(void)
{
    int ret = 0;
    uint32_t start_cycles = 0U;
    uint32_t elapsed_cycles = 0U;

    ret = app_display_init(lv_color_hex(CHOZO_DARK_BLUE));
    if (0 != ret) {
        LOG_ERR("Display initialization failed: %d", ret);
        return 0;
    }

    ret = performance_init(lv_screen_active(), PERF_PRINT_REPORT);
    if (0 != ret) {
        LOG_ERR("Performance initialization failed: %d", ret);
        return 0;
    }

    (void)lv_timer_handler();

    ret = app_display_enable();
    if (0 != ret) {
        LOG_ERR("Failed to enable display: %d", ret);
        return 0;
    }

    backlight_btn_t colors = {
        .background = lv_color_hex(CHOZO_DARK_BLUE),
        .icon = lv_color_hex(DARK_SAMUS_BLUE),
        .border = lv_color_hex(SAMUS_GREEN),
    };
    backlight_button_create(lv_screen_active(), colors, ENABLE_BTN_ANIM);

    app_display_create_version_label(lv_screen_active(), lv_color_hex(SAMUS_GREEN));

    ret = gui_wifi_init(lv_screen_active(), lv_color_hex(CHOZO_DARK_BLUE), 
                        lv_color_hex(SAMUS_GREEN));
    if (0 != ret) {
        LOG_ERR("Wi-Fi GUI initialization failed: %d", ret);
    return 0;
    }

    while (true) {
        gui_wifi_scan_process();

        start_cycles = k_cycle_get_32();

        (void)lv_timer_handler();

        elapsed_cycles = k_cycle_get_32() - start_cycles;
        performance_record_lvgl_handler(elapsed_cycles);
        performance_report();

        k_sleep(K_MSEC(MAIN_LOOP_PERIOD_MS));
    }
}
