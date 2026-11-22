/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <errno.h>
#include <stddef.h>

#include "rgb_v2_rotation.h"


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/

/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/

/*****************************************************************************
                         PRIVATE FUNCTION DECLARATIONS
*****************************************************************************/

/*****************************************************************************
                       PRIVATE FUNCTION IMPLEMENTATIONS
*****************************************************************************/

/*****************************************************************************
                           INTERFACE IMPLEMENTATION
*****************************************************************************/
void rgb_v2_rotation_dimensions(uint16_t physical_width, uint16_t physical_height,
                                enum rgb_v2_rotation rotation,
                                uint16_t *logical_width, uint16_t *logical_height)
{
    if ((RGB_V2_ROTATION_90 == rotation) || (RGB_V2_ROTATION_270 == rotation)) {
        *logical_width = physical_height;
        *logical_height = physical_width;
    } else {
        *logical_width = physical_width;
        *logical_height = physical_height;
    }
}

int rgb_v2_rotation_rect(uint16_t physical_width, uint16_t physical_height,
                         enum rgb_v2_rotation rotation, const struct rgb_v2_rect *logical,
                         struct rgb_v2_rect *physical)
{
    uint16_t logical_width = 0U;
    uint16_t logical_height = 0U;

    if ((NULL == logical) || (NULL == physical) || (RGB_V2_ROTATION_270 < rotation)) {
        return -EINVAL;
    }
    
    rgb_v2_rotation_dimensions(physical_width, physical_height, rotation,
                               &logical_width, &logical_height);
    if ((0U == logical->width) || (0U == logical->height) ||
        ((uint32_t)logical->x + logical->width > logical_width) ||
        ((uint32_t)logical->y + logical->height > logical_height)) {
        return -EINVAL;
    }

    switch (rotation) {
    case RGB_V2_ROTATION_0:
        *physical = *logical;
        break;
    case RGB_V2_ROTATION_90:
        physical->x = (uint16_t)((uint32_t)physical_width - logical->y - logical->height);
        physical->y = logical->x;
        physical->width = logical->height;
        physical->height = logical->width;
        break;
    case RGB_V2_ROTATION_180:
        physical->x = (uint16_t)((uint32_t)physical_width - logical->x - logical->width);
        physical->y = (uint16_t)((uint32_t)physical_height - logical->y - logical->height);
        physical->width = logical->width;
        physical->height = logical->height;
        break;
    case RGB_V2_ROTATION_270:
        physical->x = logical->y;
        physical->y = (uint16_t)((uint32_t)physical_height - logical->x - logical->width);
        physical->width = logical->height;
        physical->height = logical->width;
        break;
    default:
        return -EINVAL;
    }
    return 0;
}

void rgb_v2_rotation_point(uint16_t physical_width, uint16_t physical_height,
                           enum rgb_v2_rotation rotation, uint16_t logical_x,
                           uint16_t logical_y, uint16_t *physical_x, uint16_t *physical_y)
{
    switch (rotation) {
    case RGB_V2_ROTATION_90:
        *physical_x = (uint16_t)((uint32_t)physical_width - 1U - logical_y);
        *physical_y = logical_x;
        break;
    case RGB_V2_ROTATION_180:
        *physical_x = (uint16_t)((uint32_t)physical_width - 1U - logical_x);
        *physical_y = (uint16_t)((uint32_t)physical_height - 1U - logical_y);
        break;
    case RGB_V2_ROTATION_270:
        *physical_x = logical_y;
        *physical_y = (uint16_t)((uint32_t)physical_height - 1U - logical_x);
        break;
    default:
        *physical_x = logical_x;
        *physical_y = logical_y;
        break;
    }
}
