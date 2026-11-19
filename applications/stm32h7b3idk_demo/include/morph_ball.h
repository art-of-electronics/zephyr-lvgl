/**
 * @file morph_ball.h
 * @brief Morph Ball graphical demo initialization and animation.
 *
 * Provides functions for creating the Morph Ball graphical
 * interface and starting its animation.
 */

#ifndef MORPH_BALL_H
#define MORPH_BALL_H

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
 * @brief Initialize the Morph Ball demo.
 * @param [in] screen Pointer to the LVGL screen.
 * @return 0 on success, negative errno on failure.
 */
int morph_ball_init(lv_obj_t *screen);

/**
 * @brief Start the Morph Ball animation.
 * @return 0 on success, negative errno on failure.
 */
int morph_ball_run(void);

#endif // MORPH_BALL_H
