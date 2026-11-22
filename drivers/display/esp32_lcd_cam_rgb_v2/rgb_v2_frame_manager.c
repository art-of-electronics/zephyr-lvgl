/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include "rgb_v2_frame_manager.h"

#include <errno.h>
#include <stddef.h>


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
void rgb_v2_frames_init(struct rgb_v2_frame_manager *manager)
{
    uint8_t index = 0U;

    if (NULL == manager) {
        return;
    }

    for (index = 0U; index < RGB_V2_FRAME_COUNT; ++index) {
        manager->state[index] = RGB_V2_FRAME_FREE;
    }

    manager->active_index = 0U;
    manager->state[manager->active_index] = RGB_V2_FRAME_SCANNING;
    manager->pending_index = RGB_V2_FRAME_NO_PENDING;
    manager->drawing_index = RGB_V2_FRAME_NO_PENDING;
    manager->next_candidate = 1U;
    manager->requests = 0U;
    manager->latches = 0U;
    manager->acquisitions = 0U;
    manager->releases = 0U;
}

int rgb_v2_frames_acquire(struct rgb_v2_frame_manager *manager, uint8_t *index_out)
{
    uint8_t index = 0U;
    uint8_t attempt = 0U;

    if ((NULL == manager) || (NULL == index_out)) {
        return -EINVAL;
    }

    if (RGB_V2_FRAME_NO_PENDING != manager->drawing_index) {
        return -EBUSY;
    }

    for (attempt = 0U; attempt < RGB_V2_FRAME_COUNT; ++attempt) {
        index = (uint8_t)((manager->next_candidate + attempt) % RGB_V2_FRAME_COUNT);

        if (RGB_V2_FRAME_FREE == manager->state[index]) {
            manager->state[index] = RGB_V2_FRAME_DRAWING;
            manager->drawing_index = index;
            manager->next_candidate = (uint8_t)((index + 1U) % RGB_V2_FRAME_COUNT);
            manager->acquisitions++;
            *index_out = index;
            return 0;
        }
    }

    return -EBUSY;
}

int rgb_v2_frames_submit(struct rgb_v2_frame_manager *manager, uint8_t index)
{
    if ((NULL == manager) || (RGB_V2_FRAME_COUNT <= index)) {
        return -EINVAL;
    }

    if ((index != manager->drawing_index) ||
        (RGB_V2_FRAME_DRAWING != manager->state[index])) {
        return -EPERM;
    }

    if (RGB_V2_FRAME_NO_PENDING != manager->pending_index) {
        return -EBUSY;
    }

    manager->state[index] = RGB_V2_FRAME_QUEUED;
    manager->drawing_index = RGB_V2_FRAME_NO_PENDING;
    manager->pending_index = index;
    manager->requests++;

    return 0;
}

int rgb_v2_frames_cancel(struct rgb_v2_frame_manager *manager, uint8_t index)
{
    if ((NULL == manager) || (RGB_V2_FRAME_COUNT <= index)) {
        return -EINVAL;
    }

    if ((index != manager->drawing_index) ||
        (RGB_V2_FRAME_DRAWING != manager->state[index])) {
        return -EPERM;
    }

    manager->state[index] = RGB_V2_FRAME_FREE;
    manager->drawing_index = RGB_V2_FRAME_NO_PENDING;
    manager->releases++;

    return 0;
}

int rgb_v2_frames_latch_at_wrap(struct rgb_v2_frame_manager *manager)
{
    uint8_t old_active = 0U;
    uint8_t next_active = 0U;

    if (NULL == manager) {
        return -EINVAL;
    }

    old_active = manager->active_index;

    if (RGB_V2_FRAME_NO_PENDING == manager->pending_index) {
        return -ENOENT;
    }

    next_active = manager->pending_index;
    if ((RGB_V2_FRAME_COUNT <= next_active) ||
        (RGB_V2_FRAME_QUEUED != manager->state[next_active])) {
        return -EINVAL;
    }

    manager->state[old_active] = RGB_V2_FRAME_FREE;
    manager->state[next_active] = RGB_V2_FRAME_SCANNING;
    manager->active_index = next_active;
    manager->pending_index = RGB_V2_FRAME_NO_PENDING;
    manager->latches++;
    manager->releases++;

    return 0;
}

uint8_t rgb_v2_frames_active(const struct rgb_v2_frame_manager *manager)
{
    if (NULL == manager) {
        return RGB_V2_FRAME_NO_PENDING;
    }
    
    return manager->active_index;
}
