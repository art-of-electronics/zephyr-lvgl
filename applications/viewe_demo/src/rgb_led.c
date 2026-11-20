/**
 * @file rgb_led.c
 * @brief WS2812 status LED initialization.
 */

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/logging/log.h>

#include <rgb_led.h>


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
LOG_MODULE_REGISTER(viewe_rgb_led, LOG_LEVEL_INF);

#define RGB_LED_NODE DT_ALIAS(led_strip)
#define RGB_LED_PIXEL_COUNT (1U)
#define RGB_LED_START_RED   (0U)
#define RGB_LED_START_GREEN (10U)
#define RGB_LED_START_BLUE  (10U)


/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/
static const struct device *const rgb_led_dev = DEVICE_DT_GET(RGB_LED_NODE);


/*****************************************************************************
                         PRIVATE FUNCTION DECLARATIONS
*****************************************************************************/

/*****************************************************************************
                       PRIVATE FUNCTION IMPLEMENTATIONS
*****************************************************************************/

/*****************************************************************************
                           INTERFACE IMPLEMENTATION
*****************************************************************************/
int rgb_led_init(void)
{
    int ret = 0;
    struct led_rgb pixel = {
        .r = RGB_LED_START_RED,
        .g = RGB_LED_START_GREEN,
        .b = RGB_LED_START_BLUE,
    };

    if (true != device_is_ready(rgb_led_dev)) {
        LOG_ERR("RGB LED device not ready");
        return -ENODEV;
    }

    ret = led_strip_update_rgb(rgb_led_dev, &pixel, RGB_LED_PIXEL_COUNT);
    if (0 != ret) {
        LOG_ERR("RGB LED update failed: %d", ret);
    }

    return ret;
}
