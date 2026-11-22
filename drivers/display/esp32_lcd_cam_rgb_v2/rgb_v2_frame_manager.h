
#ifndef RGB_V2_FRAME_MANAGER_H
#define RGB_V2_FRAME_MANAGER_H

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <stdint.h>


/*****************************************************************************
                       PUBLIC DEFINES / MACROS / ENUMS
*****************************************************************************/
#define RGB_V2_FRAME_COUNT      (3U)
#define RGB_V2_FRAME_NO_PENDING (UINT8_MAX)

/** @brief Exclusive ownership of an RGB565 PSRAM framebuffer. */
enum rgb_v2_frame_state {
    RGB_V2_FRAME_FREE = 0,
    RGB_V2_FRAME_DRAWING,
    RGB_V2_FRAME_QUEUED,
    RGB_V2_FRAME_SCANNING,
};

/**
 * @brief Writer-safe triple framebuffer manager, independent of hardware.
 */
struct rgb_v2_frame_manager {
    enum rgb_v2_frame_state state[RGB_V2_FRAME_COUNT];
    uint8_t active_index;
    uint8_t pending_index;
    uint8_t drawing_index;
    uint8_t next_candidate; /* Round-robin search, so all three PSRAM frames are exercised. */
    uint32_t requests;
    uint32_t latches;
    uint32_t acquisitions;
    uint32_t releases;
};


/*****************************************************************************
                         PUBLIC INTERFACE DECLARATION
*****************************************************************************/
/**
 * @brief Initialize the framebuffer manager.
 * @param manager Pointer to the framebuffer manager.
 *
 * @note Framebuffer 0 starts in the SCANNING state, while the remaining
 *       two framebuffers are available for drawing.
 */
void rgb_v2_frames_init(struct rgb_v2_frame_manager *manager);

/**
 * @brief Acquire an exclusively writable framebuffer.
 * @param manager   Pointer to the framebuffer manager.
 * @param index_out Pointer receiving the acquired framebuffer index.
 * @return 0 on success, or a negative errno value on failure.
 *
 * @note Only one framebuffer can be in the DRAWING state at a time.
 *       The acquired framebuffer remains exclusively owned until
 *       it is submitted or cancelled.
 */
int rgb_v2_frames_acquire(struct rgb_v2_frame_manager *manager, uint8_t *index_out);

/**
 * @brief Submit a completed framebuffer for display.
 * @param manager Pointer to the framebuffer manager.
 * @param index   Index of the framebuffer to submit.
 * @return 0 on success, or a negative errno value on failure.
 *
 * @note The framebuffer must be fully rendered and cache-flushed before
 *       submission. On success, it transitions from DRAWING to QUEUED.
 *       If -EBUSY is returned, the caller retains ownership.
 */
int rgb_v2_frames_submit(struct rgb_v2_frame_manager *manager, uint8_t index);

/**
 * @brief Cancel an unfinished framebuffer without displaying it.
 * @param manager Pointer to the framebuffer manager.
 * @param index   Index of the framebuffer to cancel.
 * @return 0 on success, or a negative errno value on failure.
 *
 * @note Cancelling releases the DRAWING framebuffer without queuing
 *       it for scanout.
 */
int rgb_v2_frames_cancel(struct rgb_v2_frame_manager *manager, uint8_t index);

/**
 * @brief Activate a queued framebuffer at the scanout frame boundary.
 * @param manager Pointer to the framebuffer manager.
 * @return 0 on success, or a negative errno value on failure.
 *
 * @note Call immediately before refilling the first row of the next frame.
 *       All remaining rows of the previous frame must already be staged
 *       in SRAM bounce buffers, ensuring no further PSRAM reads are needed.
 *       On success, the queued framebuffer becomes SCANNING and the
 *       previously active framebuffer is released.
 */
int rgb_v2_frames_latch_at_wrap(struct rgb_v2_frame_manager *manager);

/**
 * @brief Get the framebuffer currently selected for scanout.
 * @param manager Pointer to the framebuffer manager.
 * @return Index of the active framebuffer.
 */
uint8_t rgb_v2_frames_active(const struct rgb_v2_frame_manager *manager);

#endif // RGB_V2_FRAME_MANAGER_H
