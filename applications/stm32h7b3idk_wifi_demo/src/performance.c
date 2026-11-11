#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include <lvgl.h>

#include <performance.h>
#include <theme.h>

LOG_MODULE_REGISTER(performance, LOG_LEVEL_INF);

#define PERF_REPORT_INTERVAL_MS  (1000U)
#define MS_PER_SECOND            (1000U)

#define PERF_TEXT_SIZE           (32U)
#define PERF_LABEL_X_POS         (-10)
#define PERF_LABEL_Y_POS         (10)

typedef struct {
    uint32_t handler_calls;
    uint64_t handler_total_us;
    uint64_t handler_max_us;
} perf_stats_t;

static perf_stats_t perf_stats = {0};

static atomic_t frame_count = ATOMIC_INIT(0);
static atomic_t animation_count = ATOMIC_INIT(0);

static lv_obj_t *performance_label = NULL;

static bool print_report = false;
static uint32_t previous_report_ms = 0U;

int performance_init(lv_obj_t *screen, bool enable_print_report)
{
    if (NULL == screen) {
        return -EINVAL;
    }

    print_report = enable_print_report;

    memset(&perf_stats, 0, sizeof(perf_stats));

    (void)atomic_set(&frame_count, 0);
    (void)atomic_set(&animation_count, 0);

    previous_report_ms = k_uptime_get_32();

    performance_label = lv_label_create(screen);
    if (NULL == performance_label) {
        return -ENOMEM;
    }

    lv_label_set_text(performance_label, "FPS: --\nAPS: --");
    lv_obj_set_style_text_font(performance_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(performance_label, lv_color_hex(DARK_SAMUS_BLUE), LV_PART_MAIN);
    lv_obj_set_style_text_align(performance_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_align(performance_label, LV_ALIGN_TOP_RIGHT, PERF_LABEL_X_POS, PERF_LABEL_Y_POS);

    return 0;
}

void performance_record_lvgl_handler(uint32_t elapsed_cycles)
{
    uint64_t elapsed_us = k_cyc_to_us_floor64(elapsed_cycles);

    perf_stats.handler_calls++;
    perf_stats.handler_total_us += elapsed_us;

    if (perf_stats.handler_max_us < elapsed_us) {
        perf_stats.handler_max_us = elapsed_us;
    }
}

void performance_frame_record(void)
{
    (void)atomic_inc(&frame_count);
}

void performance_animation_record(void)
{
    (void)atomic_inc(&animation_count);
}

void performance_report(void)
{
    uint32_t now_ms = k_uptime_get_32();
    uint32_t elapsed_ms = now_ms - previous_report_ms;

    uint32_t frames = 0U;
    uint32_t animations = 0U;

    uint32_t fps = 0U;
    uint32_t aps = 0U;

    uint64_t handler_avg_us = 0U;

    char performance_text[PERF_TEXT_SIZE] = {0};

    if (PERF_REPORT_INTERVAL_MS > elapsed_ms) {
        return;
    }

    frames = (uint32_t)atomic_set(&frame_count, 0);
    animations = (uint32_t)atomic_set(&animation_count, 0);

    if (0U < elapsed_ms) {
        fps = (uint32_t)(((uint64_t)frames * MS_PER_SECOND) / elapsed_ms);
        aps = (uint32_t)(((uint64_t)animations * MS_PER_SECOND) / elapsed_ms);
    }

    if (0U < perf_stats.handler_calls) {
        handler_avg_us = perf_stats.handler_total_us / perf_stats.handler_calls;
    }

    (void)snprintf(performance_text, sizeof(performance_text), 
                   "FPS: %u\nAPS: %u", fps, aps);

    if (NULL != performance_label) {
        lv_label_set_text(performance_label, performance_text);
    }

    if (true == print_report) {
        LOG_INF("FPS: %u | APS: %u | LVGL avg: %llu us | LVGL max: %llu us",
                fps, aps,
                (unsigned long long)handler_avg_us,
                (unsigned long long)perf_stats.handler_max_us);
    }

    memset(&perf_stats, 0, sizeof(perf_stats));
    previous_report_ms = now_ms;
}
