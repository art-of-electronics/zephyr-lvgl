/**
 * @file gui.h
 * @brief LVGL user interface for the CYD demo.
 *
 * Creates the themed main screen, version information, button/counter, and
 * animated Morph Ball without owning display hardware or LVGL scheduling.
 */

#ifndef GUI_H
#define GUI_H

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
 * @brief Populate an existing LVGL screen with the CYD demo interface.
 * @param screen Active LVGL screen; must not be NULL.
 * @note Call from the display/LVGL thread.
 */
void gui_init(lv_obj_t *screen);

#endif // GUI_H
