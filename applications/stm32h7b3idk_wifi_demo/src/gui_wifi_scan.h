#ifndef GUI_WIFI_SCAN_H
#define GUI_WIFI_SCAN_H

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
 * @brief Initialize the Wi-Fi scan screen.
 * @param[in] screen Parent LVGL screen.
 * @param[in] accent Accent color.
 * @retval 0 Initialization successful.
 * @retval -EINVAL Invalid screen pointer.
 * @retval -ENOMEM LVGL object allocation failed.
 */
int gui_wifi_scan_init(lv_obj_t *screen, lv_color_t accent);

/**
 * @brief Start scanning for available Wi-Fi networks.
 */
void gui_wifi_scan_start(void);

/**
 * @brief Process pending Wi-Fi scan events.
 *
 * Must be called from the LVGL application thread.
 */
void gui_wifi_scan_process(void);

/**
 * @brief Clear the Wi-Fi scan results when leaving the screen.
 */
void gui_wifi_scan_close(void);

#endif // GUI_WIFI_SCAN_H
