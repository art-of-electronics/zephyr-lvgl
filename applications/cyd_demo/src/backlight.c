/**
 * @file backlight.c
 * @brief PWM control for the CYD LCD backlight.
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
#include <zephyr/logging/log.h>

#include <backlight.h>
#include <pwm_utils.h>


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
LOG_MODULE_REGISTER(backlight, LOG_LEVEL_INF);

#define LCD_BACKLIGHT_NODE DT_ALIAS(backlight)


/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/
static const struct pwm_dt_spec lcd_backlight = PWM_DT_SPEC_GET(LCD_BACKLIGHT_NODE);


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
    if (true != pwm_is_ready_dt(&lcd_backlight)) {
        LOG_ERR("PWM controller not ready");
        return -ENODEV;
    }

    return 0;
}

int backlight_set_brightness(uint32_t percent)
{
    int ret = 0;

    ret = pwm_set_percent(&lcd_backlight, percent);
    if (0 != ret) {
        LOG_ERR("Backlight PWM failed: %d", ret);
    }

    return ret;
}
