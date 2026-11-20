/**
 * @file backlight.c
 * @brief PWM-controlled LCD backlight initialization.
 */

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>

#include <backlight.h>


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
LOG_MODULE_REGISTER(viewe_backlight, LOG_LEVEL_INF);

#define BACKLIGHT_NODE DT_ALIAS(backlight_pwm)
#define BACKLIGHT_LED_INDEX (0U)
#define BACKLIGHT_START_BRIGHTNESS_PERCENT (35U)


/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/
static const struct device *const backlight_dev = DEVICE_DT_GET(BACKLIGHT_NODE);


/*****************************************************************************
                         PRIVATE FUNCTION DECLARATIONS
*****************************************************************************/

/*****************************************************************************
                       PRIVATE FUNCTION IMPLEMENTATIONS
*****************************************************************************/

/*****************************************************************************
                           INTERFACE IMPLEMENTATION
*****************************************************************************/
int backlight_init(void)
{
    int ret = 0;

    if (true != device_is_ready(backlight_dev)) {
        LOG_ERR("Backlight device not ready");
        return -ENODEV;
    }

    ret = led_set_brightness(backlight_dev, BACKLIGHT_LED_INDEX, BACKLIGHT_START_BRIGHTNESS_PERCENT);
    if (0 != ret) {
        LOG_ERR("Backlight brightness failed: %d", ret);
    }

    return ret;
}
