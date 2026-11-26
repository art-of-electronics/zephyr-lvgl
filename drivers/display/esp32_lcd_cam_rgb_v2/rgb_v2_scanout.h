#ifndef RGB_V2_SCANOUT_H_
#define RGB_V2_SCANOUT_H_

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/drivers/dma.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <esp_intr_alloc.h>
#include <hal/dma_types.h>
#include <hal/lcd_hal.h>

#include "rgb_v2_bounce_state.h"
#include "rgb_v2_frame_manager.h"
#include "rgb_v2_dirty.h"
#include "rgb_v2_rotation.h"


/*****************************************************************************
                       PUBLIC DEFINES / MACROS / ENUMS
*****************************************************************************/
#define RGB_V2_DT_DISPLAY   DT_INST(0, espressif_esp32_lcd_cam_rgb)
#define RGB_V2_DT_TIMINGS   DT_CHILD(RGB_V2_DT_DISPLAY, display_timings)
#define RGB_V2_WIDTH        DT_PROP(RGB_V2_DT_DISPLAY, width)
#define RGB_V2_HEIGHT       DT_PROP(RGB_V2_DT_DISPLAY, height)
#define RGB_V2_PCLK         DT_PROP(RGB_V2_DT_TIMINGS, clock_frequency)

#define RGB_V2_BOUNCE_LINES     ((0U == (RGB_V2_HEIGHT % 20U)) ? (10U) : (8U))
#define RGB_V2_PIXEL_BYTES      (sizeof(uint16_t))
#define RGB_V2_FB_BYTES         (RGB_V2_WIDTH * RGB_V2_HEIGHT * RGB_V2_PIXEL_BYTES)
#define RGB_V2_BOUNCE_BYTES     (RGB_V2_WIDTH * RGB_V2_BOUNCE_LINES * RGB_V2_PIXEL_BYTES)
#define RGB_V2_DESC_BYTES       (RGB_V2_WIDTH * 2U * RGB_V2_PIXEL_BYTES)
#define RGB_V2_DESCS_PER_BOUNCE (RGB_V2_BOUNCE_BYTES / RGB_V2_DESC_BYTES)
#define RGB_V2_DESC_COUNT       (RGB_V2_DESCS_PER_BOUNCE * RGB_V2_BOUNCE_COUNT)
#define RGB_V2_EOF_PER_FRAME    (RGB_V2_HEIGHT / RGB_V2_BOUNCE_LINES)

struct rgb_v2_scanout {
    lcd_hal_context_t hal;
    intr_handle_t vsync_irq;
    const struct device *dma_dev;
    uint32_t dma_channel;
    struct dma_block_config setup_block;
    struct dma_config dma_cfg;

    dma_descriptor_t descriptors[RGB_V2_DESC_COUNT] __aligned(16);
    dma_descriptor_t restart_descriptor __aligned(16);
    uint8_t bounce[RGB_V2_BOUNCE_COUNT][RGB_V2_BOUNCE_BYTES] __aligned(64);
    uint16_t *frame_psram[RGB_V2_FRAME_COUNT];
    struct rgb_v2_frame_manager frames;
    struct rgb_v2_dirty_tracker dirty;

    /* Only thread-context display_write() uses this mutex, never the LCD ISR. */
    struct k_mutex write_mutex;
    uint8_t write_index;
    bool write_open;
    bool write_is_partial;

    struct rgb_v2_bounce_engine ownership;
    struct k_spinlock state_lock;
    uint32_t next_line;
    uint32_t eof_in_frame;
    uint32_t eof_total;
    uint32_t vsync_total;
    uint32_t dma_errors;
    uint32_t recoveries;
    uint32_t refill_max_cycles;
    bool recovery_needed;
#if defined(CONFIG_ESP32_LCD_RGB_V2_STATS)
    /* Diagnostic-only counters; updated in ISR, sampled by stats_work. */
    uint32_t short_vsync_recoveries;
    uint32_t pending_fault_recoveries;
    uint32_t pipeline_errors;
    int32_t last_pipeline_error;
    uint32_t last_vsync_eof;
    uint32_t overfull_vsync_frames;
    uint32_t last_short_eof;
    uint32_t min_short_eof;
    uint32_t max_short_eof;
    struct k_work_delayable stats_work;
#endif
};


/*****************************************************************************
                         PUBLIC INTERFACE DECLARATION
*****************************************************************************/
/**
 * @brief Initialize the ESP32-S3 LCD scanout with triple buffering.
 * @param scanout Pointer to the scanout context.
 * @return 0 on success, or a negative errno value on failure.
 *
 * @note Uses three RGB565 framebuffers in PSRAM.
 *       The initial display orientation is selected via Kconfig.
 */
int rgb_v2_scanout_init(struct rgb_v2_scanout *scanout);

/**
 * @brief Acquire an exclusively writable framebuffer.
 * @param scanout Pointer to the scanout context.
 * @param index   Pointer receiving the acquired framebuffer index.
 * @param pixels  Pointer receiving the framebuffer pixel buffer.
 * @return 0 on success, or a negative errno value on failure.
 *
 * @note Thread context only. The caller retains exclusive ownership
 *       until the framebuffer is submitted or cancelled.
 */
int rgb_v2_scanout_acquire(struct rgb_v2_scanout *scanout, uint8_t *index,
                           uint16_t **pixels);

/**
 * @brief Submit a completed framebuffer for display.
 * @param scanout Pointer to the scanout context.
 * @param index   Index of the framebuffer to submit.
 * @return 0 on success, or a negative errno value on failure.
 *
 * @note The framebuffer cache is flushed before submission.
 *       On -EBUSY, the caller retains ownership and may retry.
 *       After successful submission, the framebuffer must not be
 *       accessed until it is acquired again.
 */
int rgb_v2_scanout_submit(struct rgb_v2_scanout *scanout, uint8_t index);

/**
 * @brief Write an RGB565 rectangle to the display framebuffer.
 * @param scanout  Pointer to the scanout context.
 * @param rotation Selected display rotation.
 * @param x        Horizontal position in logical coordinates.
 * @param y        Vertical position in logical coordinates.
 * @param desc     Pointer to the display buffer descriptor.
 * @param buf      Pointer to the source pixel data.
 * @return 0 on success, or a negative errno value on failure.
 *
 * @note Supports partial framebuffer updates. The final flush
 *       publishes the completed frame for scanout.
 */
int rgb_v2_scanout_write(struct rgb_v2_scanout *scanout, enum rgb_v2_rotation rotation,
                         uint16_t x, uint16_t y,
                         const struct display_buffer_descriptor *desc, const void *buf);

/**
 * @brief Cancel an incomplete framebuffer without displaying it.
 * @param scanout Pointer to the scanout context.
 * @param index   Index of the framebuffer to cancel.
 * @return 0 on success, or a negative errno value on failure.
 *
 * @note Cancelling releases the framebuffer without queuing it
 *       for scanout.
 */
int rgb_v2_scanout_cancel(struct rgb_v2_scanout *scanout, uint8_t index);

#endif // RGB_V2_SCANOUT_H
