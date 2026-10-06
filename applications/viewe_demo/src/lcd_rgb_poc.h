/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LCD_RGB_POC_H_
#define LCD_RGB_POC_H_

/**
 * @brief Initialize the ESP32-S3 LCD_CAM peripheral in RGB mode.
 *
 * This function configures the LCD_CAM peripheral for the RGB interface
 * used by the VIEWE UEDX80480050E-WB-A display and starts the initial
 * RGB test stream.
 *
 * The implementation is intended as a proof of concept for a future
 * Zephyr display driver.
 *
 * @retval 0       Initialization completed successfully.
 * @retval -ENODEV Required hardware device is not available.
 * @retval -EINVAL Invalid or unsupported hardware configuration.
 * @retval <0      Error returned by a Zephyr subsystem.
 */
int lcd_rgb_poc_init(void);

#endif /* LCD_RGB_POC_H_ */
