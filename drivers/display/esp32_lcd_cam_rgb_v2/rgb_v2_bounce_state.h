#ifndef RGB_V2_BOUNCE_STATE_H
#define RGB_V2_BOUNCE_STATE_H

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <stdbool.h>
#include <stdint.h>


/*****************************************************************************
                       PUBLIC DEFINES / MACROS / ENUMS
*****************************************************************************/
#define RGB_V2_BOUNCE_COUNT       (2U)
#define RGB_V2_NO_ACTIVE_BOUNCE  (UINT8_MAX)

enum rgb_v2_bounce_state {
    RGB_V2_BOUNCE_EMPTY = 0,
    RGB_V2_BOUNCE_READY,
    RGB_V2_BOUNCE_SCANNING,
    RGB_V2_BOUNCE_REFILLING,
};

struct rgb_v2_refill_token {
    uint32_t epoch;
    uint64_t operation;
    uint8_t index;
};

struct rgb_v2_bounce_engine {
    enum rgb_v2_bounce_state state[RGB_V2_BOUNCE_COUNT];
    uint32_t epoch;
    uint64_t next_operation;
    uint64_t active_operation[RGB_V2_BOUNCE_COUNT];
    uint32_t deadline_misses;
    uint8_t scanning_index;
    bool fault;
};


/*****************************************************************************
                         PUBLIC INTERFACE DECLARATION
*****************************************************************************/
/**
 * @brief Reset the logical state of the bounce buffer engine.
 * @param engine Pointer to the bounce buffer engine.
 *
 * @note The caller must ensure that LCD scanout is stopped before resetting
 *       the engine. This function does not stop hardware operations.
 */
void rgb_v2_bounce_reset(struct rgb_v2_bounce_engine *engine);

/**
 * @brief Mark a bounce buffer as ready for scanout.
 * @param engine Pointer to the bounce buffer engine.
 * @param index  Index of the bounce buffer.
 * @return 0 on success, or a negative errno value on failure.
 *
 * @note The caller must complete the CPU copy and any required cache
 *       operations before marking the buffer as ready.
 */
int rgb_v2_bounce_mark_ready(struct rgb_v2_bounce_engine *engine, uint8_t index);

/**
 * @brief Start scanout using two prefilled bounce buffers.
 * @param engine      Pointer to the bounce buffer engine.
 * @param first_index Index of the first buffer to be scanned.
 * @return 0 on success, or a negative errno value on failure.
 */
int rgb_v2_bounce_start(struct rgb_v2_bounce_engine *engine, uint8_t first_index);

/**
 * @brief Complete a bounce buffer transfer and advance the scanout state.
 * @param engine          Pointer to the bounce buffer engine.
 * @param completed_index Index of the buffer whose transfer has completed.
 * @return 0 on success, or a negative errno value on failure.
 *
 * @note Call only after LCD GDMA TX has finished consuming the buffer.
 *       A late refill raises a persistent fault requiring recovery.
 */
int rgb_v2_bounce_eof(struct rgb_v2_bounce_engine *engine, uint8_t completed_index);

/**
 * @brief Reserve a bounce buffer for CPU refill.
 * @param engine Pointer to the bounce buffer engine.
 * @param index  Index of the buffer to refill.
 * @param token  Pointer to the refill token receiving reservation information.
 * @return 0 on success, or a negative errno value on failure.
 *
 * @note The caller must serialize access between ISR and thread contexts.
 *       The returned token must remain unchanged until refill completion.
 */
int rgb_v2_bounce_refill_begin(struct rgb_v2_bounce_engine *engine, uint8_t index,
                               struct rgb_v2_refill_token *token);

/**
 * @brief Complete a bounce buffer refill operation.
 * @param engine  Pointer to the bounce buffer engine.
 * @param token   Pointer to the token identifying the refill operation.
 * @param success True if the refill completed successfully, false otherwise.
 * @return 0 on success, or a negative errno value on failure.
 *
 * @note A failed refill raises a persistent fault and prevents the buffer
 *       from transitioning to the ready state.
 */
int rgb_v2_bounce_refill_finish(struct rgb_v2_bounce_engine *engine,
                                const struct rgb_v2_refill_token *token, bool success);

/**
 * @brief Cancel a reserved bounce buffer refill operation.
 * @param engine Pointer to the bounce buffer engine.
 * @param token  Pointer to the token identifying the refill operation.
 * @return 0 on success, or a negative errno value on failure.
 *
 * @note This function only updates the logical buffer state and does not
 *       stop LCD GDMA TX or any other hardware operation.
 */
int rgb_v2_bounce_refill_cancel(struct rgb_v2_bounce_engine *engine,
                                const struct rgb_v2_refill_token *token);

#endif // RGB_V2_BOUNCE_STATE_H
