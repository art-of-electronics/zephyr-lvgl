#ifndef RGB_V2_ROTATION_H
#define RGB_V2_ROTATION_H

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <stdint.h>


/*****************************************************************************
                       PUBLIC DEFINES / MACROS / ENUMS
*****************************************************************************/
enum rgb_v2_rotation {
    RGB_V2_ROTATION_0 = 0,
    RGB_V2_ROTATION_90 = 1,
    RGB_V2_ROTATION_180 = 2,
    RGB_V2_ROTATION_270 = 3,
};

struct rgb_v2_rect {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
};


/*****************************************************************************
                         PUBLIC INTERFACE DECLARATION
*****************************************************************************/
/**
 * @brief Calculate logical display dimensions for the selected rotation.
 * @param physical_width  Physical display width in pixels.
 * @param physical_height Physical display height in pixels.
 * @param rotation        Selected display rotation.
 * @param logical_width   Pointer receiving the logical display width.
 * @param logical_height  Pointer receiving the logical display height.
 */
void rgb_v2_rotation_dimensions(uint16_t physical_width, uint16_t physical_height,
                                enum rgb_v2_rotation rotation,
                                uint16_t *logical_width, uint16_t *logical_height);

/**
 * @brief Transform a rectangle from logical to physical display coordinates.
 * @param physical_width  Physical display width in pixels.
 * @param physical_height Physical display height in pixels.
 * @param rotation        Selected display rotation.
 * @param logical         Pointer to the rectangle in logical coordinates.
 * @param physical        Pointer receiving the transformed rectangle.
 * @return 0 on success, or a negative errno value on failure.
 */
int rgb_v2_rotation_rect(uint16_t physical_width, uint16_t physical_height,
                         enum rgb_v2_rotation rotation, const struct rgb_v2_rect *logical,
                         struct rgb_v2_rect *physical);

/**
 * @brief Transform a point from logical to physical display coordinates.
 * @param physical_width  Physical display width in pixels.
 * @param physical_height Physical display height in pixels.
 * @param rotation        Selected display rotation.
 * @param logical_x       Horizontal coordinate in logical space.
 * @param logical_y       Vertical coordinate in logical space.
 * @param physical_x      Pointer receiving the physical horizontal coordinate.
 * @param physical_y      Pointer receiving the physical vertical coordinate.
 */
void rgb_v2_rotation_point(uint16_t physical_width, uint16_t physical_height,
                           enum rgb_v2_rotation rotation, uint16_t logical_x,
                           uint16_t logical_y, uint16_t *physical_x, uint16_t *physical_y);

#endif // RGB_V2_ROTATION_H
