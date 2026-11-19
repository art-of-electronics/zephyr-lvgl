/**
 * @file rgb_led.c
 * @brief PWM RGB LED lifecycle and color animation.
 */

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <pwm_utils.h>
#include <rgb_led.h>


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
LOG_MODULE_REGISTER(rgb_led, LOG_LEVEL_INF);

#define LED_RED_NODE            DT_ALIAS(led0)
#define LED_GREEN_NODE          DT_ALIAS(led1)
#define LED_BLUE_NODE           DT_ALIAS(led2)
#define RGB_COLOR_MAX           (100U)
#define RGB_STEP_DELAY_MS       (20U)
#define RGB_THREAD_STACK_SIZE   (512)
#define RGB_THREAD_PRIORITY     (5)


/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/
static const struct pwm_dt_spec led_red = PWM_DT_SPEC_GET(LED_RED_NODE);
static const struct pwm_dt_spec led_green = PWM_DT_SPEC_GET(LED_GREEN_NODE);
static const struct pwm_dt_spec led_blue = PWM_DT_SPEC_GET(LED_BLUE_NODE);

static struct k_thread rgb_thread_data = {0};
K_THREAD_STACK_DEFINE(rgb_thread_stack, RGB_THREAD_STACK_SIZE);


/*****************************************************************************
                         PRIVATE FUNCTION DECLARATIONS
*****************************************************************************/

/*****************************************************************************
                       PRIVATE FUNCTION IMPLEMENTATIONS
*****************************************************************************/
static int rgb_led_init(void)
{
    if ((true != pwm_is_ready_dt(&led_red)) ||
        (true != pwm_is_ready_dt(&led_green)) ||
        (true != pwm_is_ready_dt(&led_blue))) {
        LOG_ERR("RGB PWM controller not ready");
        return -ENODEV;
    }

    LOG_INF("RGB LED initialized");

    return 0;
}

static int rgb_led_set(uint32_t red, uint32_t green, uint32_t blue)
{
    int ret = 0;

    ret = pwm_set_percent(&led_red, red);
    if (0 != ret) {
        return ret;
    }

    ret = pwm_set_percent(&led_green, green);
    if (0 != ret) {
        return ret;
    }

    return pwm_set_percent(&led_blue, blue);
}

static int rgb_led_animate_cycle(void)
{
    int ret = 0;
    uint32_t step = 0U;

    // Preserve all three original 0..100 transitions and their timing.
    for (step = 0U; step <= RGB_COLOR_MAX; step++) {
        ret = rgb_led_set(RGB_COLOR_MAX - step, step, 0U);
        if (0 != ret) {
            return ret;
        }
        k_msleep(RGB_STEP_DELAY_MS);
    }

    for (step = 0U; step <= RGB_COLOR_MAX; step++) {
        ret = rgb_led_set(0U, RGB_COLOR_MAX - step, step);
        if (0 != ret) {
            return ret;
        }
        k_msleep(RGB_STEP_DELAY_MS);
    }

    for (step = 0U; step <= RGB_COLOR_MAX; step++) {
        ret = rgb_led_set(step, 0U, RGB_COLOR_MAX - step);
        if (0 != ret) {
            return ret;
        }
        k_msleep(RGB_STEP_DELAY_MS);
    }

    return 0;
}

static void rgb_led_task(void *arg1, void *arg2, void *arg3)
{
    int ret = 0;

    (void)arg1;
    (void)arg2;
    (void)arg3;

    ret = rgb_led_init();
    if (0 != ret) {
        LOG_ERR("RGB LED initialization failed: %d", ret);
        return;
    }

    ret = rgb_led_set(0U, 0U, 0U);
    if (0 != ret) {
        LOG_ERR("RGB LED reset failed: %d", ret);
        return;
    }

    while (true) {
        ret = rgb_led_animate_cycle();
        if (0 != ret) {
            LOG_ERR("RGB animation failed: %d", ret);
            return;
        }
    }
}

/*****************************************************************************
                           INTERFACE IMPLEMENTATION
*****************************************************************************/
int rgb_led_start(void)
{
    k_tid_t tid = NULL;

    tid = k_thread_create(&rgb_thread_data, rgb_thread_stack,
                          K_THREAD_STACK_SIZEOF(rgb_thread_stack),
                          rgb_led_task, NULL, NULL, NULL,
                          RGB_THREAD_PRIORITY, 0, K_NO_WAIT);
    if (NULL == tid) {
        return -EIO;
    }

    (void)k_thread_name_set(tid, "demo_rgb");

    return 0;
}
