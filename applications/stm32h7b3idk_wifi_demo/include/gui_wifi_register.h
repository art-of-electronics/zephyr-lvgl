/**
 * @file gui_wifi_register.h
 * @brief Wi-Fi connection registration and LVGL user interface.
 *
 * Provides functions for managing network credentials through
 * an on-screen keyboard, initiating connection and disconnection,
 * and updating the interface based on Wi-Fi connection states.
 */

#ifndef GUI_WIFI_REGISTER_H
#define GUI_WIFI_REGISTER_H

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
 * @brief Create the Wi-Fi registration UI on an existing screen.
 * @param [in] screen Registration screen created by the GUI navigation module.
 * @param [in] accent Accent color used by the application theme.
 * @return 0 on success, negative errno on failure.
 */
int gui_wifi_register_init(lv_obj_t *screen, lv_color_t accent);

/**
 * @brief Refresh the registration screen when entering it.
 */
void gui_wifi_register_open(void);

/**
 * @brief Process pending Wi-Fi connection events.
 * @note Must be called periodically from the LVGL thread.
 */
void gui_wifi_register_process(void);

/**
 * @brief Clear the password and reset keyboard focus when leaving the screen.
 */
void gui_wifi_register_close(void);

#endif // GUI_WIFI_REGISTER_H
