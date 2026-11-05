#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/version.h>

#include <errno.h>
#include <lvgl.h>

LOG_MODULE_REGISTER(morph_ball, LOG_LEVEL_INF);

#define BALL_SIZE_PX    (64U)
#define BALL_RADIUS_PX  ((BALL_SIZE_PX) / 2U)
#define BALL_SPEED_PX_S (120U)
#define TIMER_PERIOD_MS (16U)
#define ANGLE_FULL_TURN (3600U)
#define PI_APPROX_MILLI (3142U)
#define MILLI_SCALE     (1000U)
#define MAX_DELTA_MS    (100U)

LV_IMAGE_DECLARE(morph_ball);

static lv_obj_t *ball;
static int32_t ball_x_milli;
static int32_t ball_y;
static int32_t direction = 1;
static int32_t angle_tenths;
static uint32_t previous_ms;
static int32_t max_x;

static void on_touch(lv_event_t *event)
{
    ARG_UNUSED(event);
    direction = -direction;
}

static void animate(lv_timer_t *timer)
{
    ARG_UNUSED(timer);
    uint32_t now = lv_tick_get();
    uint32_t delta_ms = now - previous_ms;
    previous_ms = now;
    if (MAX_DELTA_MS < delta_ms) {
        delta_ms = MAX_DELTA_MS;
    }

    int32_t dx_milli = direction * BALL_SPEED_PX_S * (int32_t)delta_ms;
    ball_x_milli += dx_milli;
    if (0 > ball_x_milli) {
        ball_x_milli = -ball_x_milli;
        direction = 1;
    } else if ((max_x * MILLI_SCALE) < ball_x_milli) {
        ball_x_milli = (2 * max_x * MILLI_SCALE) - ball_x_milli;
        direction = -1;
    }

    angle_tenths += (int32_t)(((int64_t)dx_milli * 1800) / (BALL_RADIUS_PX * PI_APPROX_MILLI));
    angle_tenths %= ANGLE_FULL_TURN;
    if (0 > angle_tenths) {
        angle_tenths += ANGLE_FULL_TURN;
    }

    lv_obj_set_pos(ball, ball_x_milli / MILLI_SCALE, ball_y);
    lv_image_set_rotation(ball, angle_tenths);
}

int main(void)
{
    int ret = 0;

    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (false == device_is_ready(display_dev))
    {
        LOG_ERR("Display device not ready");
        return 0;
    }  

    lv_display_t *display = lv_display_get_default();
    if (NULL == display) {
        LOG_ERR("No LVGL display");
        return 0;
    }

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x07101B), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(screen, false);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "MORPH BALL");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFAA42), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    char version_text[64] = {0};
    (void)snprintf(version_text, sizeof(version_text),
			       "Zephyr %u.%u x LVGL %u.%u",
			       (unsigned int)KERNEL_VERSION_MAJOR,
			       (unsigned int)KERNEL_VERSION_MINOR,
			       (unsigned int)lv_version_major(),
			       (unsigned int)lv_version_minor());

    lv_obj_t *version_label = lv_label_create(screen);
    lv_label_set_text(version_label, version_text);
	lv_obj_set_style_text_font(version_label, &lv_font_montserrat_40, LV_PART_MAIN);
	lv_obj_set_style_text_color(version_label, lv_color_hex(0xF07818), LV_PART_MAIN);
	lv_obj_set_style_text_align(version_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
	lv_obj_center(version_label);

    lv_obj_t *floor = lv_obj_create(screen);
    lv_obj_set_size(floor, lv_display_get_horizontal_resolution(display), 3);
    lv_obj_set_pos(floor, 0, lv_display_get_vertical_resolution(display) - 34);
    lv_obj_set_style_bg_color(floor, lv_color_hex(0xF07818), 0);
    lv_obj_set_style_border_width(floor, 0, 0);
    lv_obj_set_style_radius(floor, 0, 0);
    lv_obj_set_scrollable(floor, false);

    ball = lv_image_create(screen);
    lv_image_set_src(ball, &morph_ball);
    lv_image_set_pivot(ball, BALL_RADIUS_PX, BALL_RADIUS_PX);
    lv_obj_set_clickable(ball, true);
    lv_obj_add_event_cb(ball, on_touch, LV_EVENT_CLICKED, NULL);

    max_x = lv_display_get_horizontal_resolution(display) - BALL_SIZE_PX;
    ball_y = lv_display_get_vertical_resolution(display) - 34 - BALL_SIZE_PX;
    ball_x_milli = 0;
    previous_ms = lv_tick_get();
    lv_obj_set_pos(ball, 0, ball_y);
    lv_timer_create(animate, TIMER_PERIOD_MS, NULL);
    LOG_INF("Morph Ball demo started");

    (void)lv_timer_handler();

    ret = display_blanking_off(display_dev);
    if ((0 > ret) && (-ENOSYS != ret))
    {
        LOG_ERR("Failed to disable display blanking: %d", ret);
        return 0;
    }

    while (true)
    {
        (void)lv_timer_handler();
        k_sleep(K_MSEC(10));
    }
}
