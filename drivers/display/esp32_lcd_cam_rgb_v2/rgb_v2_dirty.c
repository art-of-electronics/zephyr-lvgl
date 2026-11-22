/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include "rgb_v2_dirty.h"

#include <stdbool.h>
#include <errno.h>
#include <string.h>


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
#define RGB_V2_BITS_PER_BYTE (8U)


/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/

/*****************************************************************************
                         PRIVATE FUNCTION DECLARATIONS
*****************************************************************************/

/*****************************************************************************
                       PRIVATE FUNCTION IMPLEMENTATIONS
*****************************************************************************/
static uint32_t rgb_v2_tile_count(const struct rgb_v2_dirty_tracker *tracker)
{
    return (uint32_t)tracker->tile_columns * (uint32_t)tracker->tile_rows;
}

static void rgb_v2_mask_set(uint8_t *mask, uint32_t tile)
{
    mask[tile / RGB_V2_BITS_PER_BYTE] |= (uint8_t)(1U << (tile % RGB_V2_BITS_PER_BYTE));
}

static bool rgb_v2_mask_get(const uint8_t *mask, uint32_t tile)
{
    return (0U != (mask[tile / RGB_V2_BITS_PER_BYTE] & (uint8_t)(1U << (tile % RGB_V2_BITS_PER_BYTE))));
}

static void rgb_v2_mask_fill(uint8_t *mask, uint32_t count)
{
    uint32_t tile = 0U;

    memset(mask, 0, RGB_V2_DIRTY_MASK_BYTES);
    for (tile = 0U; tile < count; ++tile) {
        rgb_v2_mask_set(mask, tile);
    }
}


/*****************************************************************************
                           INTERFACE IMPLEMENTATION
*****************************************************************************/
int rgb_v2_dirty_init(struct rgb_v2_dirty_tracker *tracker, uint16_t width, uint16_t height)
{
    uint8_t index = 0U;
    uint32_t tiles_x = 0U;
    uint32_t tiles_y = 0U;

    if ((NULL == tracker) || (0U == width) || (0U == height)) {
        return -EINVAL;
    }

    tiles_x = ((uint32_t)width + RGB_V2_DIRTY_TILE_WIDTH - 1U) / RGB_V2_DIRTY_TILE_WIDTH;
    tiles_y = ((uint32_t)height + RGB_V2_DIRTY_TILE_HEIGHT - 1U) / RGB_V2_DIRTY_TILE_HEIGHT;

    if ((tiles_x * tiles_y) > RGB_V2_DIRTY_MAX_TILES) {
        return -E2BIG;
    }

    memset(tracker, 0, sizeof(*tracker));
    tracker->width = width;
    tracker->height = height;
    tracker->tile_columns = (uint16_t)tiles_x;
    tracker->tile_rows = (uint16_t)tiles_y;

    for (index = 1U; index < RGB_V2_FRAME_COUNT; ++index) {
        rgb_v2_mask_fill(tracker->stale[index], rgb_v2_tile_count(tracker));
    }

    return 0;
}

int rgb_v2_dirty_sync(struct rgb_v2_dirty_tracker *tracker, uint8_t destination,
                      const uint16_t *source_pixels, uint16_t *destination_pixels)
{
    uint32_t tile = 0U;
    uint32_t row = 0U;
    uint32_t tx = 0U;
    uint32_t ty = 0U;
    uint32_t left = 0U;
    uint32_t top = 0U;
    uint32_t columns = 0U;
    uint32_t rows = 0U;
    size_t offset = 0U;

    if ((NULL == tracker) || (NULL == source_pixels) || (NULL == destination_pixels) ||
        (destination >= RGB_V2_FRAME_COUNT) || (source_pixels == destination_pixels)) {
        return -EINVAL;
    }

    for (tile = 0U; tile < rgb_v2_tile_count(tracker); ++tile) {
        if (false == rgb_v2_mask_get(tracker->stale[destination], tile)) {
            continue;
        }

        tx = tile % tracker->tile_columns;
        ty = tile / tracker->tile_columns;
        left = tx * RGB_V2_DIRTY_TILE_WIDTH;
        top = ty * RGB_V2_DIRTY_TILE_HEIGHT;
        columns = ((uint32_t)tracker->width - left < RGB_V2_DIRTY_TILE_WIDTH) ?
                  ((uint32_t)tracker->width - left) : (RGB_V2_DIRTY_TILE_WIDTH);
        rows = ((uint32_t)tracker->height - top < RGB_V2_DIRTY_TILE_HEIGHT) ?
               ((uint32_t)tracker->height - top) : (RGB_V2_DIRTY_TILE_HEIGHT);

        for (row = 0U; row < rows; ++row) {
            offset = ((size_t)top + row) * tracker->width + left;
            memcpy(&destination_pixels[offset], &source_pixels[offset], (size_t)columns * sizeof(uint16_t));
        }
    }

    memset(tracker->stale[destination], 0, RGB_V2_DIRTY_MASK_BYTES);
    memset(tracker->changed, 0, RGB_V2_DIRTY_MASK_BYTES);

    return 0;
}

int rgb_v2_dirty_record(struct rgb_v2_dirty_tracker *tracker, uint16_t x, uint16_t y,
                        uint16_t width, uint16_t height)
{
    uint32_t start_x = 0U;
    uint32_t start_y = 0U;
    uint32_t end_x = 0U;
    uint32_t end_y = 0U;
    uint32_t tile_x = 0U;
    uint32_t tile_y = 0U;

    if ((NULL == tracker) || (0U == width) || (0U == height) ||
        ((uint32_t)x + width > tracker->width) ||
        ((uint32_t)y + height > tracker->height)) {
        return -EINVAL;
    }

    start_x = x / RGB_V2_DIRTY_TILE_WIDTH;
    start_y = y / RGB_V2_DIRTY_TILE_HEIGHT;
    end_x = ((uint32_t)x + width - 1U) / RGB_V2_DIRTY_TILE_WIDTH;
    end_y = ((uint32_t)y + height - 1U) / RGB_V2_DIRTY_TILE_HEIGHT;

    for (tile_y = start_y; tile_y <= end_y; ++tile_y) {
        for (tile_x = start_x; tile_x <= end_x; ++tile_x) {
            rgb_v2_mask_set(tracker->changed, tile_y * tracker->tile_columns + tile_x);
        }
    }

    return 0;
}

void rgb_v2_dirty_record_full(struct rgb_v2_dirty_tracker *tracker)
{
    if (NULL != tracker) {
        rgb_v2_mask_fill(tracker->changed, rgb_v2_tile_count(tracker));
    }
}

int rgb_v2_dirty_publish(struct rgb_v2_dirty_tracker *tracker, uint8_t submitted)
{
    uint8_t index = 0U;
    uint32_t byte = 0U;

    if ((NULL == tracker) || (submitted >= RGB_V2_FRAME_COUNT)) {
        return -EINVAL;
    }

    for (index = 0U; index < RGB_V2_FRAME_COUNT; ++index) {
        if (index == submitted) {
            memset(tracker->stale[index], 0, RGB_V2_DIRTY_MASK_BYTES);
        } else {
            for (byte = 0U; byte < RGB_V2_DIRTY_MASK_BYTES; ++byte) {
                tracker->stale[index][byte] |= tracker->changed[byte];
            }
        }
    }

    memset(tracker->changed, 0, RGB_V2_DIRTY_MASK_BYTES);
    return 0;
}

void rgb_v2_dirty_cancel(struct rgb_v2_dirty_tracker *tracker, uint8_t destination)
{
    if ((NULL == tracker) || (destination >= RGB_V2_FRAME_COUNT)) {
        return;
    }
    
    rgb_v2_mask_fill(tracker->stale[destination], rgb_v2_tile_count(tracker));
    memset(tracker->changed, 0, RGB_V2_DIRTY_MASK_BYTES);
}
