/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(cyd_demo, LOG_LEVEL_INF);

#define LED_RED_NODE        DT_ALIAS(led0)
#define LED_GREEN_NODE      DT_ALIAS(led1)
#define LED_BLUE_NODE       DT_ALIAS(led2)
#define LCD_BACKLIGHT_NODE  DT_ALIAS(backlight)

#define PWM_MAX_BRIGHTNESS  (100U)
#define PWM_STEP_DELAY_MS   (20U)
#define PWM_COLOR_STEPS     (100U)
#define PWM_BACKLIGHT_MIN   (10U)

static const struct pwm_dt_spec led_red = PWM_DT_SPEC_GET(LED_RED_NODE);
static const struct pwm_dt_spec led_green = PWM_DT_SPEC_GET(LED_GREEN_NODE);
static const struct pwm_dt_spec led_blue = PWM_DT_SPEC_GET(LED_BLUE_NODE);
static const struct pwm_dt_spec lcd_backlight = PWM_DT_SPEC_GET(LCD_BACKLIGHT_NODE);

static int pwm_set_brightness(const struct pwm_dt_spec *pwm, uint32_t brightness)
{
    uint64_t pulse = 0U;

    if (PWM_MAX_BRIGHTNESS < brightness) {
        return -EINVAL;
    }

    pulse = ((uint64_t)pwm->period * brightness) / PWM_MAX_BRIGHTNESS;

    return pwm_set_dt(pwm, pwm->period, (uint32_t)pulse);
}

static int pwm_init(void)
{
    if ((false == pwm_is_ready_dt(&led_red)) ||
        (false == pwm_is_ready_dt(&led_green)) ||
        (false == pwm_is_ready_dt(&led_blue)) ||
        (false == pwm_is_ready_dt(&lcd_backlight))) {
        LOG_ERR("PWM controller not ready");
        return -ENODEV;
    }

    return 0;
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

    return pwm_set_brightness(&led_blue, blue);
    if (0 != ret) {
        return ret;
    }
}

int main(void)
{
    int ret = 0;
    uint32_t step = 0U;

    ret = pwm_init();

    if (0 != ret) {
        return ret;
    }

    LOG_INF("CYD ESP32-035 PWM demo started");

    ret = pwm_set_brightness(&lcd_backlight, 35U);
    if (0 != ret) {
        LOG_ERR("Backlight PWM failed: %d", ret);
        return ret;
    }

    while (true) {
        for (step = 0U; step <= PWM_COLOR_STEPS; step++) {
            ret = pwm_set_rgb(PWM_COLOR_STEPS - step, step, 0U);
            if (0 != ret) {
                LOG_ERR("RGB PWM failed: %d", ret);
                return ret;
            }

            k_msleep(PWM_STEP_DELAY_MS);
        }

        for (step = 0U; step <= PWM_COLOR_STEPS; step++) {
            ret = pwm_set_rgb(0U, PWM_COLOR_STEPS - step, step);
            if (0 != ret) {
                LOG_ERR("RGB PWM failed: %d", ret);
                return ret;
            }

            k_msleep(PWM_STEP_DELAY_MS);
        }

        for (step = 0U; step <= PWM_COLOR_STEPS; step++) {
            ret = pwm_set_rgb(step, 0U, PWM_COLOR_STEPS - step);

            if (0 != ret) {
                LOG_ERR("RGB PWM failed: %d", ret);
                return ret;
            }

            k_msleep(PWM_STEP_DELAY_MS);
        }
    }

    return 0;
}
