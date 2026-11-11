#include <backlight.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(backlight, LOG_LEVEL_INF);

#define BACKLIGHT_NODE DT_NODELABEL(lcd_backlight)
#define BACKLIGHT_LOW_PERCENT   (35U)
#define BACKLIGHT_HIGH_PERCENT  (85U)
#define BACKLIGHT_PERCENT_MAX   (100U)

static bool backlight_high = true;
static lv_obj_t *backlight_button = NULL;
static lv_obj_t *backlight_icon = NULL;

static const struct pwm_dt_spec backlight = PWM_DT_SPEC_GET(BACKLIGHT_NODE);

static void backlight_button_cb(lv_event_t *event)
{
    int ret = 0;
    uint32_t brightness = BACKLIGHT_LOW_PERCENT;

    ARG_UNUSED(event);

    brightness = (true == backlight_high) ? (BACKLIGHT_LOW_PERCENT) : (BACKLIGHT_HIGH_PERCENT);
    ret = backlight_set_brightness(brightness);
    if (0 != ret) {
        LOG_ERR("Backlight PWM error: %d", ret);
        return;
    }

    backlight_high = !backlight_high;
    lv_label_set_text(backlight_icon, (true == backlight_high) ? (LV_SYMBOL_EYE_OPEN) : (LV_SYMBOL_EYE_CLOSE));
}

int backlight_set_brightness(uint32_t percent)
{
    uint32_t pulse_ns = 0U;

    if (BACKLIGHT_PERCENT_MAX < percent) {
        return -EINVAL;
    }

    pulse_ns = (uint32_t)(((uint64_t)backlight.period * percent) / BACKLIGHT_PERCENT_MAX);
    return pwm_set_dt(&backlight, backlight.period, pulse_ns);
}

uint32_t backlight_get_brightness(void)
{
    return (true == backlight_high) ? (BACKLIGHT_HIGH_PERCENT) : (BACKLIGHT_LOW_PERCENT);
}

void backlight_button_create(lv_obj_t *screen, backlight_btn_t colors, bool enable_anim)
{
    backlight_button = lv_button_create(screen);

    if (true != enable_anim) {
        lv_obj_remove_style_all(backlight_button);
    }

    lv_obj_set_size(backlight_button, 42, 42);
    lv_obj_align(backlight_button, LV_ALIGN_TOP_LEFT, 8, 8);
    lv_obj_set_style_radius(backlight_button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(backlight_button, colors.background, 0);
    lv_obj_set_style_bg_opa(backlight_button, LV_OPA_COVER, 0);

    lv_obj_set_style_border_width(backlight_button, 2, 0);
    lv_obj_set_style_border_color(backlight_button, colors.border, 0);

    lv_obj_add_event_cb(backlight_button, backlight_button_cb, LV_EVENT_CLICKED, NULL);

    backlight_icon = lv_label_create(backlight_button);
    lv_label_set_text(backlight_icon, LV_SYMBOL_EYE_OPEN);
    lv_obj_center(backlight_icon);
    lv_obj_set_style_text_color(backlight_icon, colors.icon, 0);
}

int backlight_init(void) 
{
    int ret = 0;

    if (true != pwm_is_ready_dt(&backlight)) {
        LOG_ERR("Backlight PWM device not ready");
        return -ENODEV;
    }

    ret = backlight_set_brightness(BACKLIGHT_HIGH_PERCENT);
    if (0 != ret) {
        LOG_ERR("Backlight PWM error: %d", ret);
    }

    return ret;
}