/**
 * @file display.h
 * @brief Display initialization and LVGL user interface setup.
 *
 * Provides functions for initializing the application display,
 * configuring the LVGL screen, and enabling the LCD backlight.
 */

#ifndef DISPLAY_H
#define DISPLAY_H

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <lvgl.h>


/*****************************************************************************
                       PUBLIC DEFINES / MACROS / ENUMS
*****************************************************************************/

/*****************************************************************************
                         PUBLIC INTERFACE DECLARATION
*****************************************************************************/
/**
 * @brief Initialize the display user interface.
 * @param [in] color_bg Screen backgroud color.
 * @retval 0 Initialization completed successfully.
 * @retval -ENODEV Display device or LVGL display is unavailable.
 */
int app_display_init(lv_color_t color_bg);

/**
 * @brief Enable the display and initialize its backlight.
 * @retval 0 Display enabled successfully.
 * @retval <0 Failed to disable display blanking.
 */
int app_display_enable(void);

/**
 * @brief Create an LVGL label displaying Zephyr and LVGL versions.
 * @param [in] screen Parent LVGL object.
 * @param [in] text_color Text color.
 */
void app_display_create_version_label(lv_obj_t *screen, lv_color_t text_color);

#endif // DISPLAY_H
