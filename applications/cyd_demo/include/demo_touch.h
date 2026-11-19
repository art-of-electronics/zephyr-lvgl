/**
 * @file demo_touch.h
 * @brief Optional raw touchscreen diagnostic helper.
 *
 * Declares the legacy input-event logger, which is intentionally excluded
 * from the normal build. Touchscreen operation is handled by Zephyr/LVGL.
 */

#ifndef DEMO_TOUCH_H
#define DEMO_TOUCH_H

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
 * @brief Check that the raw touchscreen input device is available.
 * @return 0 when ready, negative errno value on failure.
 */
int demo_touch_init(void);

#endif // DEMO_TOUCH_H
