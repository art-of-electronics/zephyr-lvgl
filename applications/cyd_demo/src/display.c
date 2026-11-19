/**
 * @file display.c
 * @brief Display lifecycle and dedicated LVGL thread.
 */

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <lvgl.h>

#include <cyd_st7796s.h>
#include <backlight.h>
#include <display.h>
#include <gui.h>


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
LOG_MODULE_REGISTER(display_app, LOG_LEVEL_INF);

#define DISPLAY_THREAD_STACK_SIZE   (8192)
#define DISPLAY_THREAD_PRIORITY     (5)
#define DISPLAY_TASK_DELAY_MS       (10U)
#define BACKLIGHT_DEFAULT_PERCENT   (35U)


/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/
static struct k_thread display_thread_data = {0};
K_THREAD_STACK_DEFINE(display_thread_stack, DISPLAY_THREAD_STACK_SIZE);

static const struct device *const display_device = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));


/*****************************************************************************
                         PRIVATE FUNCTION DECLARATIONS
*****************************************************************************/

/*****************************************************************************
                       PRIVATE FUNCTION IMPLEMENTATIONS
*****************************************************************************/
static int display_init(void)
{
    int ret = 0;
    struct display_capabilities capabilities = {0};
    lv_obj_t *screen = NULL;

    if (true != device_is_ready(display_device)) {
        LOG_ERR("Display device not ready");
        return -ENODEV;
    }

    display_get_capabilities(display_device, &capabilities);

    LOG_INF("Display device: %s", display_device->name);
    LOG_INF("Display: %u x %u", capabilities.x_resolution, capabilities.y_resolution);
    LOG_INF("Pixel format: 0x%x", capabilities.current_pixel_format);

    // Keep the existing panel configuration and display API sequence intact.
    ret = cyd_st7796s_configure();
    if (0 != ret) {
        LOG_ERR("LCD configuration failed: %d", ret);
        return ret;
    }

    ret = display_blanking_off(display_device);
    if (0 != ret) {
        LOG_ERR("Display blanking off failed: %d", ret);
        return ret;
    }

    screen = lv_screen_active();
    if (NULL == screen) {
        LOG_ERR("LVGL active screen is NULL");
        return -ENODEV;
    }

    LOG_INF("LCD RGB started");
    gui_init(screen);

    return 0;
}

static void display_task(void *arg1, void *arg2, void *arg3)
{
    int ret = 0;

    (void)arg1;
    (void)arg2;
    (void)arg3;

    ret = backlight_init();
    if (0 != ret) {
        LOG_ERR("LCD PWM initialization failed: %d", ret);
        return;
    }

    ret = backlight_set_brightness(BACKLIGHT_DEFAULT_PERCENT);
    if (0 != ret) {
        LOG_ERR("LCD brightness configuration failed: %d", ret);
        return;
    }

    ret = display_init();
    if (0 != ret) {
        LOG_ERR("LCD initialization failed: %d", ret);
        return;
    }

    while (true) {
        (void)lv_timer_handler();
        k_msleep(DISPLAY_TASK_DELAY_MS);
    }
}


/*****************************************************************************
                           INTERFACE IMPLEMENTATION
*****************************************************************************/
int display_start(void)
{
    k_tid_t tid = NULL;

    tid = k_thread_create(&display_thread_data, display_thread_stack,
                          K_THREAD_STACK_SIZEOF(display_thread_stack),
                          display_task, NULL, NULL, NULL,
                          DISPLAY_THREAD_PRIORITY, 0, K_NO_WAIT);
    if (NULL == tid) {
        return -EIO;
    }

    (void)k_thread_name_set(tid, "demo_lcd");

    return 0;
}
