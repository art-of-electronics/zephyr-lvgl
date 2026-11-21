/**
 * @file main.c
 * @brief VIEWE LVGL demonstration application entry point.
 */

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <backlight.h>
#include <display.h>
#include <gui.h>
#include <rgb_led.h>


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
LOG_MODULE_REGISTER(viewe_main, LOG_LEVEL_INF);

#define LVGL_TASK_DELAY_MS (2U)


/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/

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
    lv_obj_t *screen = NULL;

    ret = display_init(&screen);
    if (0 != ret) {
        return ret;
    }

    ret = rgb_led_init();
    if (0 != ret) {
        return ret;
    }

    ret = backlight_init();
    if (0 != ret) {
        return ret;
    }

    ret = gui_init(screen);
    if (0 != ret) {
        LOG_ERR("GUI initialization failed: %d", ret);
        return ret;
    }

    while (true) {
        display_process();
        k_sleep(K_MSEC(LVGL_TASK_DELAY_MS));
    }

    return 0;
}
