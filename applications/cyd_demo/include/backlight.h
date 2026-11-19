/**
 * @file backlight.h
 * @brief PWM-controlled CYD display backlight.
 *
 * Provides initialization and brightness control for the onboard LCD backlight.
 */

#ifndef BACKLIGHT_H
#define BACKLIGHT_H

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <stdint.h>


/*****************************************************************************
                       PUBLIC DEFINES / MACROS / ENUMS
*****************************************************************************/

/*****************************************************************************
                         PUBLIC INTERFACE DECLARATION
*****************************************************************************/
/**
 * @brief Check availability of the LCD backlight PWM controller.
 * @retval 0 The controller is ready.
 * @retval -ENODEV The controller is unavailable.
 */
int backlight_init(void);

/**
 * @brief Set the LCD backlight brightness.
 * @param percent Brightness in the range 0 to 100 percent.
 * @return 0 on success, negative errno value on failure.
 */
int backlight_set_brightness(uint32_t percent);

#endif // BACKLIGHT_H
