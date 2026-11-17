/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <lvgl.h>

#include <display.h>
#include <backlight.h>
#include <performance.h>
#include <theme.h>

#include <morph_ball.h>


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
LOG_MODULE_REGISTER(app_main, LOG_LEVEL_INF);

#define MAIN_LOOP_PERIOD_MS (2U)
#define PERF_PRINT_REPORT   (true)
#define ENABLE_BTN_ANIM     (false)


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

    ret = app_display_init(lv_color_hex(CHOZO_DARK_BLUE));
    if (0 != ret) {
        LOG_ERR("Display initialization failed: %d", ret);
        return 0;
    }

    (void)lv_timer_handler();

    ret = app_display_enable();
    if (0 != ret) {
        LOG_ERR("Failed to enable display: %d", ret);
        return 0;
    }

    backlight_btn_t colors = {
        .background = lv_color_hex(CHOZO_DARK_BLUE),
        .icon = lv_color_hex(SAMUS_YELLOW),
        .border = lv_color_hex(SAMUS_VARIA_ORANGE),
    };
    backlight_button_create(lv_screen_active(), colors, ENABLE_BTN_ANIM);

    app_display_create_version_label(lv_screen_active(), lv_color_hex(SAMUS_VARIA_ORANGE));
    
    ret = performance_init(lv_screen_active(), PERF_PRINT_REPORT);
    if (0 != ret) {
        LOG_ERR("Performance initialization failed: %d", ret);
        return 0;
    }

    ret = morph_ball_init(lv_screen_active());
    if (0 != ret) {
        LOG_ERR("Demo initialization failed: %d", ret);
        return 0;
    }

    ret = morph_ball_run();
    if (0 != ret) {
        LOG_ERR("Demo start failed: %d", ret);
        return 0;
    }

    while (true) {
        uint32_t start_cycles = 0U;
        uint32_t elapsed_cycles = 0U;

        performance_report();

        start_cycles = k_cycle_get_32();

        (void)lv_timer_handler();

        elapsed_cycles = k_cycle_get_32() - start_cycles;

        performance_record_lvgl_handler(elapsed_cycles);

        k_sleep(K_MSEC(MAIN_LOOP_PERIOD_MS));
    }
}
