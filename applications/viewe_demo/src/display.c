/**
 * @file display.c
 * @brief Zephyr display readiness checks and LVGL processing.
 */

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>

#include <display.h>


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
LOG_MODULE_REGISTER(viewe_display, LOG_LEVEL_INF);

#define DISPLAY_NODE DT_CHOSEN(zephyr_display)


/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/
static const struct device *const display_dev = DEVICE_DT_GET(DISPLAY_NODE);


/*****************************************************************************
                         PRIVATE FUNCTION DECLARATIONS
*****************************************************************************/

/*****************************************************************************
                       PRIVATE FUNCTION IMPLEMENTATIONS
*****************************************************************************/

/*****************************************************************************
                           INTERFACE IMPLEMENTATION
*****************************************************************************/
int display_init(lv_obj_t **screen)
{
    lv_obj_t *active_screen = NULL;

    if (NULL == screen) {
        return -EINVAL;
    }

    *screen = NULL;

    if (true != device_is_ready(display_dev)) {
        LOG_ERR("Display device not ready");
        return -ENODEV;
    }

    if (NULL == lv_display_get_default()) {
        LOG_ERR("LVGL display not available");
        return -ENODEV;
    }

    active_screen = lv_screen_active();
    if (NULL == active_screen) {
        LOG_ERR("LVGL active screen is NULL");
        return -ENODEV;
    }

    *screen = active_screen;
    LOG_INF("LCD RGB started");
    return 0;
}

void display_process(void)
{
    (void)lv_timer_handler();
}
