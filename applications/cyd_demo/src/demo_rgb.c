#include <demo_rgb.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo_rgb, LOG_LEVEL_INF);

#define LED_RED_NODE        DT_ALIAS(led0)
#define LED_GREEN_NODE      DT_ALIAS(led1)
#define LED_BLUE_NODE       DT_ALIAS(led2)

#define PWM_MAX_BRIGHTNESS  (100U)
#define PWM_STEP_DELAY_MS   (20U)
#define PWM_COLOR_STEPS     (100U)

static const struct pwm_dt_spec led_red = PWM_DT_SPEC_GET(LED_RED_NODE);
static const struct pwm_dt_spec led_green = PWM_DT_SPEC_GET(LED_GREEN_NODE);
static const struct pwm_dt_spec led_blue = PWM_DT_SPEC_GET(LED_BLUE_NODE);

static int pwm_set_brightness(const struct pwm_dt_spec *pwm, uint32_t brightness)
{
    uint64_t pulse = 0U;

    if (PWM_MAX_BRIGHTNESS < brightness) {
        return -EINVAL;
    }

    pulse = ((uint64_t)pwm->period * brightness) / PWM_MAX_BRIGHTNESS;

    return pwm_set_dt(pwm, pwm->period, (uint32_t)pulse);
}

static int pwm_set_rgb(uint32_t red, uint32_t green, uint32_t blue)
{
    int ret = 0;

    ret = pwm_set_brightness(&led_red, red);
    if (0 != ret) {
        return ret;
    }

    ret = pwm_set_brightness(&led_green, green);
    if (0 != ret) {
        return ret;
    }

    ret = pwm_set_brightness(&led_blue, blue);
    if (0 != ret) {
        return ret;
    }

    return 0;
}

int demo_rgb_init(void)
{
    if ((true != pwm_is_ready_dt(&led_red)) ||
        (true != pwm_is_ready_dt(&led_green)) ||
        (true != pwm_is_ready_dt(&led_blue))) {
            LOG_ERR("PWM controller not ready");
            return -ENODEV;
    }

    LOG_INF("RGB Led init() successfull");

    return 0;
}

int demo_rgb_set_rgb(uint32_t red, uint32_t green, uint32_t blue)
{
    int ret = pwm_set_rgb(red, green, blue);
    if (0 != ret) {
        LOG_ERR("RGB PWM failed: %d", ret);
        return ret;
    }

    return 0;
}

void demo_rgb_loop(void) 
{
    uint32_t step = 0U;

    for (step = 0U; step <= PWM_COLOR_STEPS; step++) {
        (void)demo_rgb_set_rgb(PWM_COLOR_STEPS - step, step, 0U);
        k_msleep(PWM_STEP_DELAY_MS);
    }

    for (step = 0U; step <= PWM_COLOR_STEPS; step++) {
        (void)demo_rgb_set_rgb(0U, PWM_COLOR_STEPS - step, step);
        k_msleep(PWM_STEP_DELAY_MS);
    }

    for (step = 0U; step <= PWM_COLOR_STEPS; step++) {
        (void)demo_rgb_set_rgb(step, 0U, PWM_COLOR_STEPS - step);
        k_msleep(PWM_STEP_DELAY_MS);
    }
}
