#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/version.h>

#include <lvgl.h>

LOG_MODULE_REGISTER(morph_ball, LOG_LEVEL_INF);

#define PI_APPROX_MILLI (3142U)
#define MILLI_SCALE     (1000U)
#define MAX_DELTA_MS    (100U)

#define TIMER_PERIOD_MS (16U)
#define ANGLE_FULL_TURN (3600U)

#define FLOOR_OFFSET_PX (34U)
#define FLOOR_HEIGHT_PX (3U)
#define BALL_SIZE_PX    (64U)
#define BALL_RADIUS_PX  ((BALL_SIZE_PX) / 2U)
#define BALL_CIRCUMFERENCE_MILLI (2U * BALL_RADIUS_PX * PI_APPROX_MILLI)

#define BALL_SPEED_MIN_PX_S     (60U)
#define BALL_SPEED_MAX_PX_S     (180U)
#define BALL_ACCEL_PX_S2        (240U)
#define BALL_DECEL_PX_S2        (360U)
#define BALL_BRAKING_ZONE_PX    (96U)

#define GLOW_WIDTH_PX      (48)
#define GLOW_HEIGHT_PX     (8)
#define GLOW_OFFSET_Y_PX   (3)
#define GLOW_PULSE_STEP    (2U)
#define GLOW_OPA_MIN       (40U)
#define GLOW_OPA_MAX       (180U)

#define IMPACT_DURATION_MS (250U)
#define IMPACT_RECOLOR_MAX (180U)

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

typedef struct
{
    uint32_t frames;
    uint32_t animations;
    uint32_t handler_calls;
    uint64_t handler_total_us;
    uint64_t handler_max_us;
} perf_stats_t;

static perf_stats_t perf_stats = {0};

LV_IMAGE_DECLARE(morph_ball);

static lv_obj_t *ball = NULL;
static int32_t ball_x_milli = 0;
static int32_t ball_y = 0;
static int32_t direction = 1;
static int32_t angle_tenths = 0;
static uint32_t previous_ms = 0U;
static int32_t max_x = 0;
static int32_t ball_speed_milli = (int32_t)(BALL_SPEED_MIN_PX_S * MILLI_SCALE);

static lv_obj_t *glow = NULL;
static uint8_t glow_opacity = GLOW_OPA_MIN;
static int32_t glow_direction = 1;

static uint32_t impact_remaining_ms = 0U;

static lv_obj_t *performance_label = NULL;

static void performance_callback(lv_event_t *event)
{
    if (LV_EVENT_REFR_READY == lv_event_get_code(event)) {
        perf_stats.frames++;
    }
}

static void on_touch(lv_event_t *event)
{
    ARG_UNUSED(event);
    direction = -direction;
}

static void update_ball_glow(void)
{
    int32_t glow_x = (ball_x_milli / MILLI_SCALE) + ((BALL_SIZE_PX - GLOW_WIDTH_PX) / 2U);
    int32_t glow_y = ball_y + BALL_SIZE_PX - GLOW_OFFSET_Y_PX;

    lv_obj_set_pos(glow, glow_x, glow_y);

    if (0 < glow_direction) {
        if (GLOW_OPA_MAX <= glow_opacity + GLOW_PULSE_STEP) {
            glow_opacity = GLOW_OPA_MAX;
            glow_direction = -1;
        } else {
            glow_opacity += GLOW_PULSE_STEP;
        }
    } else {
        if (GLOW_OPA_MIN >= glow_opacity - GLOW_PULSE_STEP) {
            glow_opacity = GLOW_OPA_MIN;
            glow_direction = 1;
        } else {
            glow_opacity -= GLOW_PULSE_STEP;
        }
    }

    lv_obj_set_style_bg_opa(glow, glow_opacity, 0);
}

static int32_t get_target_speed(void)
{
    int32_t distance_px = 0;
    int32_t target_speed = (int32_t)BALL_SPEED_MAX_PX_S;

    if (0 < direction) {
        distance_px = max_x - (ball_x_milli / (int32_t)MILLI_SCALE);
    } else {
        distance_px = ball_x_milli / (int32_t)MILLI_SCALE;
    }

    if (0 > distance_px) {
        distance_px = 0;
    }

    if ((int32_t)BALL_BRAKING_ZONE_PX > distance_px) {
        target_speed = (int32_t)BALL_SPEED_MIN_PX_S +
                       (((int32_t)(BALL_SPEED_MAX_PX_S - BALL_SPEED_MIN_PX_S) * distance_px) / 
                        (int32_t)BALL_BRAKING_ZONE_PX);
    }

    return target_speed * (int32_t)MILLI_SCALE;
}

static void update_ball_speed(uint32_t delta_ms)
{
    int32_t target_speed = get_target_speed();
    int32_t speed_step = 0;

    if (ball_speed_milli < target_speed) {
        speed_step = (int32_t)(BALL_ACCEL_PX_S2 * delta_ms);

        ball_speed_milli += speed_step;

        if (target_speed < ball_speed_milli) {
            ball_speed_milli = target_speed;
        }
    } else if (ball_speed_milli > target_speed) {
        speed_step = (int32_t)(BALL_DECEL_PX_S2 * delta_ms);

        ball_speed_milli -= speed_step;

        if (target_speed > ball_speed_milli) {
            ball_speed_milli = target_speed;
        }
    }
}

static void trigger_impact(void)
{
    impact_remaining_ms = IMPACT_DURATION_MS;

    lv_obj_set_style_image_recolor_opa(ball, (lv_opa_t)IMPACT_RECOLOR_MAX, 0);
}

static void update_impact_effect(uint32_t delta_ms)
{
    uint32_t opacity = 0U;

    if (0U == impact_remaining_ms) {
        return;
    }

    if (impact_remaining_ms <= delta_ms) {
        impact_remaining_ms = 0U;

        lv_obj_set_style_image_recolor_opa(ball, LV_OPA_TRANSP, 0);

        return;
    }

    impact_remaining_ms -= delta_ms;
    opacity = (IMPACT_RECOLOR_MAX * impact_remaining_ms) / IMPACT_DURATION_MS;

    lv_obj_set_style_image_recolor_opa(ball, (lv_opa_t)opacity, 0);
}

static void animate(lv_timer_t *timer)
{
    ARG_UNUSED(timer);
    perf_stats.animations++;
    uint32_t now = lv_tick_get();
    uint32_t delta_ms = now - previous_ms;
    previous_ms = now;
    if (MAX_DELTA_MS < delta_ms) {
        delta_ms = MAX_DELTA_MS;
    }

    update_ball_speed(delta_ms);

    int32_t dx_milli = (int32_t)(((int64_t)direction * ball_speed_milli * 
                                  (int32_t)delta_ms) / (int32_t)MILLI_SCALE);
    ball_x_milli += dx_milli;

    if (0 > ball_x_milli) {
        ball_x_milli = -ball_x_milli;
        direction = 1;
        ball_speed_milli = (int32_t)(BALL_SPEED_MIN_PX_S * MILLI_SCALE);
        trigger_impact();
    } else if ((max_x * (int32_t)MILLI_SCALE) < ball_x_milli) {
        ball_x_milli = (2 * max_x * (int32_t)MILLI_SCALE) - ball_x_milli;
        direction = -1;
        ball_speed_milli = (int32_t)(BALL_SPEED_MIN_PX_S * MILLI_SCALE);
        trigger_impact();
    }

    angle_tenths = (int32_t)(((int64_t)ball_x_milli * ANGLE_FULL_TURN) / (BALL_CIRCUMFERENCE_MILLI));
    angle_tenths %= ANGLE_FULL_TURN;
    if (0 > angle_tenths) {
        angle_tenths += ANGLE_FULL_TURN;
    }

    lv_obj_set_pos(ball, (ball_x_milli / (MILLI_SCALE)), ball_y);
    lv_image_set_rotation(ball, angle_tenths);

    update_ball_glow();
    update_impact_effect(delta_ms);
}

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

    LOG_INF("FPS: %u | APS: %u | LVGL avg: %llu us | LVGL max: %llu us",
            fps, aps,
            (unsigned long long)handler_avg_us,
            (unsigned long long)perf_stats.handler_max_us);

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

    lv_display_add_event_cb(display, performance_callback, LV_EVENT_REFR_READY, NULL);

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(CHOZO_DARK_BLUE), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(screen, false);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "MORPH BALL");
    lv_obj_set_style_text_color(title, lv_color_hex(SAMUS_YELLOW), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    char version_text[64] = {0};
    (void)snprintf(version_text, sizeof(version_text),
			       "Zephyr %u.%u x LVGL %u.%u",
			       (unsigned int)KERNEL_VERSION_MAJOR, (unsigned int)KERNEL_VERSION_MINOR,
			       (unsigned int)lv_version_major(), (unsigned int)lv_version_minor());

    performance_label = lv_label_create(screen);
    lv_label_set_text(performance_label, "FPS: --\nAPS: --");
    lv_obj_set_style_text_font(performance_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(performance_label, lv_color_hex(DARK_SAMUS_BLUE), LV_PART_MAIN);
    lv_obj_set_style_text_align(performance_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_align(performance_label, LV_ALIGN_TOP_RIGHT, -10, 10);

    lv_obj_t *version_label = lv_label_create(screen);
    lv_label_set_text(version_label, version_text);
	lv_obj_set_style_text_font(version_label, &lv_font_montserrat_40, LV_PART_MAIN);
	lv_obj_set_style_text_color(version_label, lv_color_hex(SAMUS_VARIA_ORANGE), LV_PART_MAIN);
	lv_obj_set_style_text_align(version_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
	lv_obj_center(version_label);

    lv_obj_t *floor = lv_obj_create(screen);
    lv_obj_set_size(floor, lv_display_get_horizontal_resolution(display), FLOOR_HEIGHT_PX);
    lv_obj_set_pos(floor, 0, lv_display_get_vertical_resolution(display) - FLOOR_OFFSET_PX);
    lv_obj_set_style_bg_color(floor, lv_color_hex(CHOZO_GOLD), 0);
    lv_obj_set_style_border_width(floor, 0, 0);
    lv_obj_set_style_radius(floor, 0, 0);
    lv_obj_set_scrollable(floor, false);

    ball = lv_image_create(screen);
    lv_image_set_src(ball, &morph_ball);
    lv_image_set_pivot(ball, BALL_RADIUS_PX, BALL_RADIUS_PX);
    lv_obj_set_clickable(ball, true);
    lv_obj_add_event_cb(ball, on_touch, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_image_recolor(ball, lv_color_hex(SAMUS_YELLOW), 0);
    lv_obj_set_style_image_recolor_opa(ball, LV_OPA_TRANSP, 0);

    glow = lv_obj_create(screen);
    lv_obj_set_size(glow, GLOW_WIDTH_PX, GLOW_HEIGHT_PX);
    lv_obj_set_style_bg_color(glow, lv_color_hex(SAMUS_GREEN), 0);
    lv_obj_set_style_bg_opa(glow, glow_opacity, 0);
    lv_obj_set_style_border_width(glow, 0, 0);
    lv_obj_set_style_outline_width(glow, 0, 0);
    lv_obj_set_style_pad_all(glow, 0, 0);
    lv_obj_set_style_radius(glow, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_scrollable(glow, false);
    lv_obj_set_clickable(glow, false);

    max_x = lv_display_get_horizontal_resolution(display) - BALL_SIZE_PX;
    ball_y = lv_display_get_vertical_resolution(display) - FLOOR_OFFSET_PX - BALL_SIZE_PX;
    ball_x_milli = 0;
    previous_ms = lv_tick_get();
    lv_obj_set_pos(ball, 0, ball_y);
    lv_timer_create(animate, TIMER_PERIOD_MS, NULL);
    LOG_INF("Morph Ball demo started");

    (void)lv_timer_handler();

    ret = display_blanking_off(display_dev);
    if ((0 > ret) && (-ENOSYS != ret)) {
        LOG_ERR("Failed to disable display blanking: %d", ret);
        return 0;
    }

    while (true) {
        process_lvgl();
        report_performance();
        k_sleep(K_MSEC(10));
    }
}
