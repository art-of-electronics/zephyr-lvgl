/**
 * @file rgb_led.h
 * @brief Three-channel PWM RGB LED demonstration.
 *
 * Starts a dedicated Zephyr thread that continuously cycles through the
 * onboard RGB LED colors.
 */

#ifndef RGB_LED_H
#define RGB_LED_H

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
 * @brief Start the RGB LED animation thread.
 * @return 0 on success, negative errno value if thread creation fails.
 * @note PWM initialization errors are reported asynchronously by the thread.
 */
int rgb_led_start(void);

#endif // RGB_LED_H
