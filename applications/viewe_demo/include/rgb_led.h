/**
 * @file rgb_led.h
 * @brief RGB LED initialization for the VIEWE demo.
 * 
 * Sets fixed color on RGB-LED
 */

#ifndef VIEWE_DEMO_RGB_LED_H
#define VIEWE_DEMO_RGB_LED_H

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
 * @brief Set the status RGB LED to the demo's startup color.
 * @return 0 on success, negative errno code otherwise.
 */
int rgb_led_init(void);

#endif // VIEWE_DEMO_RGB_LED_H
