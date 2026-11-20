/**
 * @file display.h
 * @brief Display validation and LVGL runtime support for the VIEWE demo.
 *
 * Provides functions for obtaining the active LVGL screen and processing
 * LVGL timers in the application's main loop.
 */

#ifndef VIEWE_DEMO_DISPLAY_H
#define VIEWE_DEMO_DISPLAY_H

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
 * @brief Validate the Zephyr display and obtain the active LVGL screen.
 * @param screen Active LVGL screen on successful initialization.
 * @return 0 on success, negative errno code otherwise.
 */
int display_init(lv_obj_t **screen);

/** @brief Run a single LVGL timer-handler iteration. */
void display_process(void);

#endif // VIEWE_DEMO_DISPLAY_H
