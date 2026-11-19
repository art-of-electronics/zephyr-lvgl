/**
 * @file pwm_utils.h
 * @brief Shared percentage-to-PWM duty-cycle helper.
 *
 * Converts brightness percentages to the pulse width required by Zephyr PWM.
 */

#ifndef PWM_UTILS_H
#define PWM_UTILS_H

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <stdint.h>
#include <zephyr/drivers/pwm.h>


/*****************************************************************************
                       PUBLIC DEFINES / MACROS / ENUMS
*****************************************************************************/

/*****************************************************************************
                         PUBLIC INTERFACE DECLARATION
*****************************************************************************/
/**
 * @brief Set a PWM output duty cycle as a percentage of its configured period.
 * @param pwm Devicetree PWM specification.
 * @param percent Duty cycle in the range 0 to 100 percent.
 * @return 0 on success, negative errno value on failure.
 */
int pwm_set_percent(const struct pwm_dt_spec *pwm, uint32_t percent);

#endif // PWM_UTILS_H
