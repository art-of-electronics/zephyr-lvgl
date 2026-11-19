/**
 * @file demo_touch.c
 * @brief Optional raw XPT2046 input-event diagnostic logger.
 */

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <errno.h>
#include <stdbool.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

#include <demo_touch.h>


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
LOG_MODULE_REGISTER(demo_touch, LOG_LEVEL_INF);

#define TOUCH_NODE DT_NODELABEL(touchscreen)


/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/
static const struct device *const touch_dev = DEVICE_DT_GET(TOUCH_NODE);


/*****************************************************************************
                         PRIVATE FUNCTION DECLARATIONS
*****************************************************************************/

/*****************************************************************************
                       PRIVATE FUNCTION IMPLEMENTATIONS
*****************************************************************************/
static void touch_callback(struct input_event *event, void *user_data)
{
    ARG_UNUSED(user_data);

    if (INPUT_ABS_X == event->code) {
        LOG_INF("Touch X: %d", event->value);
    } else if (INPUT_ABS_Y == event->code) {
        LOG_INF("Touch Y: %d", event->value);
    } else if (INPUT_BTN_TOUCH == event->code) {
        LOG_INF("Touch: %s", (0 != event->value) ? ("PRESSED") : ("RELEASED"));
    }
}


INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(TOUCH_NODE), touch_callback, NULL);


/*****************************************************************************
                           INTERFACE IMPLEMENTATION
*****************************************************************************/
int demo_touch_init(void)
{
    if (true != device_is_ready(touch_dev)) {
        LOG_ERR("XPT2046 device not ready");
        return -ENODEV;
    }

    LOG_INF("XPT2046 ready");

    return 0;
}
