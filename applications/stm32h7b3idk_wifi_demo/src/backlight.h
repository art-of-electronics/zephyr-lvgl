/**
 * @file backlight.h
 * @brief LCD backlight control and LVGL user interface.
 *
 * Provides an interface for initializing the LCD backlight
 * and creating an LVGL button to control its brightness.
 */

#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include <stdint.h>

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
 * @param [in] screen Parent LVGL object.
 * @param [in] colors Color configuration for the button.
 * @param [in] enable_anim Enable button animation.
 */
void backlight_button_create(lv_obj_t *screen, backlight_btn_t colors, bool enable_anim);

/**
 * @brief Initialize the LCD backlight.
 * @return 0 Initialization completed successfully, error code if initialization fails.
 */
int backlight_init(void);

/**
 * @brief Set LCD backlight brightness.
 *
 * @param[in] percent Brightness in the range 0-100%.
 *
 * @retval 0 Brightness updated successfully.
 * @retval <0 PWM configuration failed.
 */
int backlight_set_brightness(uint32_t percent);

/**
 * @brief Get the user-selected backlight brightness.
 *
 * @return Brightness percentage.
 */
uint32_t backlight_get_brightness(void);

#endif //BACKLIGHT_H
