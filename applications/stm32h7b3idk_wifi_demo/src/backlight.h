/**
 * @file backlight.h
 * @brief LCD backlight control and LVGL user interface.
 *
 * Provides an interface for initializing the LCD backlight
 * and creating an LVGL button to control its brightness.
 */

#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include <lvgl.h>

/**
 * @brief Color configuration for the backlight control button.
 * @note Defines the colors used by the LVGL backlight button and its graphical elements.
 */
typedef struct {
    lv_color_t background;
    lv_color_t icon;
    lv_color_t border;
} backlight_btn_t;

/**
 * @brief Create an LVGL button for controlling the LCD backlight.
 * @param[in] screen Parent LVGL object.
 * @param[in] colors Color configuration for the button.
 */
void backlight_button_create(lv_obj_t *screen, backlight_btn_t colors);

/**
 * @brief Initialize the LCD backlight.
 * @return 0 Initialization completed successfully, error code if initialization fails.
 */
int backlight_init(void);

#endif //BACKLIGHT_H