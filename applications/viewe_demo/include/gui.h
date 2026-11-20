/**
 * @file gui.h
 * @brief Shared LVGL interface for both VIEWE display variants.
 *
 * Creates the demo widgets and selects a size-aware layout using the
 * active LVGL display dimensions.
 */

#ifndef VIEWE_DEMO_GUI_H
#define VIEWE_DEMO_GUI_H

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
 * @brief Create the demo interface on the supplied LVGL screen.
 * @param screen Active LVGL screen.
 * @return 0 on success, negative errno code otherwise.
 */
int gui_init(lv_obj_t *screen);

#endif // VIEWE_DEMO_GUI_H
