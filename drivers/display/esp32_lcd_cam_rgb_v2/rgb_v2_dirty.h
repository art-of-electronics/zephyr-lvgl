#ifndef RGB_V2_DIRTY_H
#define RGB_V2_DIRTY_H

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <stdint.h>

#include "rgb_v2_frame_manager.h"


/*****************************************************************************
                       PUBLIC DEFINES / MACROS / ENUMS
*****************************************************************************/
#define RGB_V2_DIRTY_TILE_WIDTH    (32U)
#define RGB_V2_DIRTY_TILE_HEIGHT   (16U)
#define RGB_V2_DIRTY_MAX_TILES     (1024U)
#define RGB_V2_DIRTY_MASK_BYTES    (RGB_V2_DIRTY_MAX_TILES / 8U)

struct rgb_v2_dirty_tracker {
    uint16_t width;
    uint16_t height;
    uint16_t tile_columns;
    uint16_t tile_rows;
    uint8_t stale[RGB_V2_FRAME_COUNT][RGB_V2_DIRTY_MASK_BYTES];
    uint8_t changed[RGB_V2_DIRTY_MASK_BYTES];
};


/*****************************************************************************
                         PUBLIC INTERFACE DECLARATION
*****************************************************************************/
/**
 * @brief Initialize the dirty tile tracker.
 * @param tracker Pointer to the dirty tile tracker.
 * @param width   Display width in pixels.
 * @param height  Display height in pixels.
 * @return 0 on success, or a negative errno value on failure.
 *
 * @note Framebuffer 0 is initially considered complete, while the remaining
 *       framebuffers require synchronization.
 */
int rgb_v2_dirty_init(struct rgb_v2_dirty_tracker *tracker,
                      uint16_t width, uint16_t height);

/**
 * @brief Synchronize obsolete tiles in the destination framebuffer.
 * @param tracker            Pointer to the dirty tile tracker.
 * @param destination        Index of the destination framebuffer.
 * @param source_pixels      Pointer to the complete source framebuffer.
 * @param destination_pixels Pointer to the destination framebuffer.
 * @return 0 on success, or a negative errno value on failure.
 *
 * @note Only obsolete tiles are copied. The source framebuffer must remain
 *       complete and unchanged throughout the synchronization.
 */
int rgb_v2_dirty_sync(struct rgb_v2_dirty_tracker *tracker, uint8_t destination,
                      const uint16_t *source_pixels, uint16_t *destination_pixels);

/**
 * @brief Record a modified rectangle in the drawing framebuffer.
 * @param tracker Pointer to the dirty tile tracker.
 * @param x       Horizontal position of the rectangle in pixels.
 * @param y       Vertical position of the rectangle in pixels.
 * @param width   Rectangle width in pixels.
 * @param height  Rectangle height in pixels.
 *
 * @return 0 on success, or a negative errno value on failure.
 */
int rgb_v2_dirty_record(struct rgb_v2_dirty_tracker *tracker, uint16_t x, uint16_t y,
                        uint16_t width, uint16_t height);

/**
 * @brief Mark all tiles as modified.
 * @param tracker Pointer to the dirty tile tracker.
 *
 * @note Intended for direct full-frame submissions.
 */
void rgb_v2_dirty_record_full(struct rgb_v2_dirty_tracker *tracker);

/**
 * @brief Publish modified tiles after a successful framebuffer submission.
 * @param tracker   Pointer to the dirty tile tracker.
 * @param submitted Index of the submitted framebuffer.
 * @return 0 on success, or a negative errno value on failure.
 *
 * @note Updates the tile generations of the remaining framebuffers
 *       to track which regions require synchronization.
 */
int rgb_v2_dirty_publish(struct rgb_v2_dirty_tracker *tracker, uint8_t submitted);

/**
 * @brief Cancel an incomplete framebuffer update.
 * @param tracker     Pointer to the dirty tile tracker.
 * @param destination Index of the framebuffer containing incomplete data.
 *
 * @note The cancelled framebuffer requires full synchronization before
 *       it can be reused for drawing.
 */
void rgb_v2_dirty_cancel(struct rgb_v2_dirty_tracker *tracker, uint8_t destination);

#endif // RGB_V2_DIRTY_H
