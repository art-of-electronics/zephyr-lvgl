#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/version.h>

#include <lvgl.h>
#include <backlight.h>

LOG_MODULE_REGISTER(wifi_main, LOG_LEVEL_INF);

#define PERF_REPORT_INTERVAL_MS (1000U)
#define MS_PER_SECOND           (1000U)

#define SAMUS_VARIA_ORANGE  (0xF07818U)
#define SAMUS_YELLOW        (0xFFAA42U)
#define SAMUS_RED       	(0xD62828U)
#define SAMUS_BLUE      	(0x2389DAU)
#define SAMUS_GREEN     	(0x50C878U)
#define SAMUS_PURPLE    	(0x8445C7U)

#define DARK_SAMUS_BLUE     (0x00CFFFU)
#define DARK_SAMUS_ARMOR	(0x142B5AU)

#define CHOZO_DARK_BLUE     (0x07101BU)
#define CHOZO_GOLD          (0xB89B5EU)

#define WIFI_NODE DT_NODELABEL(eswifi0)

typedef struct
{
    uint32_t frames;
    uint32_t animations;
    uint32_t handler_calls;
    uint64_t handler_total_us;
    uint64_t handler_max_us;
} perf_stats_t;

static perf_stats_t perf_stats = {0};

static lv_obj_t *performance_label = NULL;

static void process_lvgl(void)
{
    uint32_t start_cycles = k_cycle_get_32();
    uint32_t elapsed_cycles = 0U;
    uint64_t elapsed_us = 0U;

    (void)lv_timer_handler();

    elapsed_cycles = k_cycle_get_32() - start_cycles;
    elapsed_us = k_cyc_to_us_floor64(elapsed_cycles);

    perf_stats.handler_calls++;
    perf_stats.handler_total_us += elapsed_us;

    if (perf_stats.handler_max_us < elapsed_us) {
        perf_stats.handler_max_us = elapsed_us;
    }
}

static void report_performance(void)
{
    static uint32_t previous_report_ms = 0U;

    uint32_t now_ms = k_uptime_get_32();
    uint32_t elapsed_ms = now_ms - previous_report_ms;

    uint32_t fps = 0U;
    uint32_t aps = 0U;

    uint64_t handler_avg_us = 0U;

    if (PERF_REPORT_INTERVAL_MS > elapsed_ms) {
        return;
    }

    if (0U < elapsed_ms) {
        fps = (uint32_t)(((uint64_t)perf_stats.frames * MS_PER_SECOND) / elapsed_ms);
        aps = (uint32_t)(((uint64_t)perf_stats.animations * MS_PER_SECOND) / elapsed_ms);
    }

    if (0U < perf_stats.handler_calls) {
        handler_avg_us = perf_stats.handler_total_us / perf_stats.handler_calls;
    }
/*
    LOG_INF("FPS: %u | APS: %u | LVGL avg: %llu us | LVGL max: %llu us",
            fps, aps,
            (unsigned long long)handler_avg_us,
            (unsigned long long)perf_stats.handler_max_us);
*/
    char performance_text[32] = {0};
    (void)snprintf(performance_text, sizeof(performance_text), 
                   "FPS: %u\nAPS: %u", fps, aps);
    lv_label_set_text(performance_label, performance_text);

    memset(&perf_stats, 0, sizeof(perf_stats));
    previous_report_ms = now_ms;
}

int main(void)
{
    int ret = 0;

    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (false == device_is_ready(display_dev)) {
        LOG_ERR("Display device not ready");
        return 0;
    }  

    lv_display_t *display = lv_display_get_default();
    if (NULL == display) {
        LOG_ERR("No LVGL display");
        return 0;
    }

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(CHOZO_DARK_BLUE), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(screen, false);

    backlight_btn_t colors = {
        .background = lv_color_hex(CHOZO_DARK_BLUE),
        .icon = lv_color_hex(DARK_SAMUS_BLUE),
        .border = lv_color_hex(SAMUS_GREEN),
    };
    backlight_button_create(lv_screen_active(), colors);

    char version_text[64] = {0};
    (void)snprintf(version_text, sizeof(version_text),
			       "Zephyr %u.%u x LVGL %u.%u",
			       (unsigned int)KERNEL_VERSION_MAJOR, (unsigned int)KERNEL_VERSION_MINOR,
			       (unsigned int)lv_version_major(), (unsigned int)lv_version_minor());

    lv_obj_t *version_label = lv_label_create(screen);
    lv_label_set_text(version_label, version_text);
	lv_obj_set_style_text_font(version_label, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(version_label, lv_color_hex(SAMUS_GREEN), LV_PART_MAIN);
	lv_obj_set_style_text_align(version_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(version_label, LV_ALIGN_TOP_MID, 0, 12);

    performance_label = lv_label_create(screen);
    lv_label_set_text(performance_label, "FPS: --\nAPS: --");
    lv_obj_set_style_text_font(performance_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(performance_label, lv_color_hex(DARK_SAMUS_BLUE), LV_PART_MAIN);
    lv_obj_set_style_text_align(performance_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_align(performance_label, LV_ALIGN_TOP_RIGHT, -10, 10);

    (void)lv_timer_handler();

    ret = display_blanking_off(display_dev);
    if ((0 > ret) && (-ENOSYS != ret)) {
        LOG_ERR("Failed to disable display blanking: %d", ret);
        return 0;
    }

    ret = backlight_init();
    if (0 != ret) {
        LOG_ERR("Backlight initialization failed: %d", ret);
    }

    while (true) {
        process_lvgl();
        report_performance();

        k_sleep(K_MSEC(100));
    }
}
