#ifndef RGB_V2_DISPLAY_CONTROL_H
#define RGB_V2_DISPLAY_CONTROL_H


/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <stdint.h>
#include <zephyr/device.h>


/*****************************************************************************
                       PUBLIC DEFINES / MACROS / ENUMS
*****************************************************************************/

/*****************************************************************************
                         PUBLIC INTERFACE DECLARATION
*****************************************************************************/
/**
 * @brief Acquire an exclusively writable RGB565 framebuffer in PSRAM.
 * @param dev    Pointer to the display device.
 * @param index  Pointer receiving the acquired framebuffer index.
 * @param pixels Pointer receiving the framebuffer pixel buffer.
 * @return 0 on success, or a negative errno value on failure.
 *
 * @note Thread context only. Only one framebuffer can be acquired for
 *       writing at a time. The caller retains exclusive ownership until
 *       the framebuffer is submitted or cancelled.
 *       LCD scanout never reads from a framebuffer being drawn.
 */
int rgb_v2_display_acquire(const struct device *dev, uint8_t *index, uint16_t **pixels);

/**
 * @brief Submit a completed framebuffer for display.
 * @param dev   Pointer to the display device.
 * @param index Index of the framebuffer to submit.
 * @return 0 on success, or a negative errno value on failure.
 *
 * @note The framebuffer cache is flushed before submission.
 *       On -EBUSY, the caller retains ownership and may retry.
 *       After successful submission, the framebuffer must not be
 *       accessed until it is acquired again.
 */
int rgb_v2_display_submit(const struct device *dev, uint8_t index);

/**
 * @brief Cancel an unfinished framebuffer without displaying it.
 * @param dev   Pointer to the display device.
 * @param index Index of the framebuffer to cancel.
 * @return 0 on success, or a negative errno value on failure.
 *
 * @note Cancelling releases the caller's exclusive ownership
 *       without submitting the framebuffer for scanout.
 */
int rgb_v2_display_cancel(const struct device *dev, uint8_t index);

#endif // RGB_V2_DISPLAY_CONTROL_H
