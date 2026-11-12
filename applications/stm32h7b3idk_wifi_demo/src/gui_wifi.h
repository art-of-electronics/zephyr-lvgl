/**
 * @file gui_wifi.h
 * @brief LVGL user interface for Wi-Fi management.
 */

#ifndef GUI_WIFI_H
#define GUI_WIFI_H

#include <lvgl.h>

/**
 * @brief Initialize the Wi-Fi management interface.
 *
 * @param[in] screen Main LVGL screen.
 * @param[in] background Background color.
 * @param[in] accent Accent color for buttons and labels.
 *
 * @retval 0 Initialization completed successfully.
 * @retval -EINVAL Invalid screen pointer.
 * @retval -ENOMEM Failed to create an LVGL object.
 * @retval -EALREADY Interface already initialized.
 */
int gui_wifi_init(lv_obj_t *screen, lv_color_t background, lv_color_t accent);

#endif // GUI_WIFI_H
