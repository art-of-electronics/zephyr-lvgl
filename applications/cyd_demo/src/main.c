/**
 * @file main.c
 * @brief Application entry point for the CYD ESP32-035 demo.
 *
 * Starts the independent RGB and display threads and periodically samples
 * the ambient light sensor. Peripheral implementations live in their modules.
 */

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <display.h>
#include <ldr.h>
#include <rgb_led.h>

/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
LOG_MODULE_REGISTER(cyd_demo, LOG_LEVEL_INF);

#define LDR_SAMPLING_PERIOD K_SECONDS(1)


/*****************************************************************************
                         PRIVATE FUNCTION DECLARATIONS
*****************************************************************************/

/*****************************************************************************
                       PRIVATE FUNCTION IMPLEMENTATIONS
*****************************************************************************/

/*****************************************************************************
                           INTERFACE IMPLEMENTATION
*****************************************************************************/
int main(void)
{
    int ret = 0;

    ret = ldr_init();
    if (0 != ret) {
        LOG_ERR("LDR initialization failed: %d", ret);
        return ret;
    }

    ret = rgb_led_start();
    if (0 != ret) {
        LOG_ERR("RGB thread creation failed: %d", ret);
        return ret;
    }

    ret = display_start();
    if (0 != ret) {
        LOG_ERR("Display thread creation failed: %d", ret);
        return ret;
    }

    while (true) {
        ret = ldr_sample();
        if (0 != ret) {
            LOG_WRN("LDR measurement failed: %d", ret);
        }

        k_sleep(LDR_SAMPLING_PERIOD);
    }

    return 0;
}
