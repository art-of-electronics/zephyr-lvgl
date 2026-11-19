/**
 * @file display.h
 * @brief CYD display initialization and LVGL application thread.
 *
 * Configures the existing ST7796S panel integration, starts the graphical
 * interface, and services LVGL in a dedicated Zephyr thread.
 */

#ifndef DISPLAY_H
#define DISPLAY_H

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/

/*****************************************************************************
                       PUBLIC DEFINES / MACROS / ENUMS
*****************************************************************************/

/*****************************************************************************
                         PUBLIC INTERFACE DECLARATION
*****************************************************************************/
/**
 * @brief Start the display/LVGL thread.
 * @return 0 on success, negative errno value if thread creation fails.
 * @note Device initialization errors are reported asynchronously by the thread.
 */
int display_start(void);

#endif // DISPLAY_H
