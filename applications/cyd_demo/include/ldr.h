/**
 * @file ldr.h
 * @brief Ambient light measurement through the ESP32 ADC.
 *
 * Configures the Devicetree-defined LDR channel, samples it, and logs
 * the raw ADC code and converted millivolts.
 */

#ifndef LDR_H
#define LDR_H

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
 * @brief Prepare the LDR ADC channel and conversion sequence.
 * @return 0 on success, negative errno value on failure.
 */
int ldr_init(void);

/**
 * @brief Read the LDR and log its raw and millivolt values.
 * @return 0 on success, negative errno value on failure.
 * @note Call only after successful ldr_init().
 */
int ldr_sample(void);

#endif // LDR_H
