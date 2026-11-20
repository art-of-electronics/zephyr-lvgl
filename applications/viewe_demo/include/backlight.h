/**
 * @file backlight.h
 * @brief LCD backlight initialization for the VIEWE demo.
 * 
 * Provides initialization for the onboard LCD backlight.
 */

#ifndef VIEWE_DEMO_BACKLIGHT_H
#define VIEWE_DEMO_BACKLIGHT_H

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
 * @brief Initialize the LCD backlight with the default brightness.
 * @return 0 on success, a negative errno code otherwise.
 */
int backlight_init(void);

#endif // VIEWE_DEMO_BACKLIGHT_H
