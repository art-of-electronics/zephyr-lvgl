/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include "rgb_v2_bounce_state.h"

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
static bool rgb_v2_valid_index(uint8_t index)
{
    return (RGB_V2_BOUNCE_COUNT > index);
}

static bool rgb_v2_valid_token(const struct rgb_v2_bounce_engine *engine, 
                               const struct rgb_v2_refill_token *token)
{
    return ((NULL != engine) && (NULL != token) && (true == rgb_v2_valid_index(token->index)) &&
            (engine->epoch == token->epoch) && (0U != token->operation) &&
            (engine->active_operation[token->index] == token->operation));
}


/*****************************************************************************
                           INTERFACE IMPLEMENTATION
*****************************************************************************/
void rgb_v2_bounce_reset(struct rgb_v2_bounce_engine *engine)
{
    uint8_t index = 0U;

    if (NULL == engine) {
        return;
    }

    engine->epoch++;
    if (0U == engine->epoch) {
        engine->epoch = 1U;
    }

    for (index = 0U; index < RGB_V2_BOUNCE_COUNT; ++index) {
        engine->state[index] = RGB_V2_BOUNCE_EMPTY;
        engine->active_operation[index] = 0U;
    }

    engine->scanning_index = RGB_V2_NO_ACTIVE_BOUNCE;
    engine->fault = false;
}

int rgb_v2_bounce_mark_ready(struct rgb_v2_bounce_engine *engine, uint8_t index)
{
    if ((NULL == engine) || (false == rgb_v2_valid_index(index))) {
        return -EINVAL;
    }

    if (true == engine->fault) {
        return -EIO;
    }

    if (RGB_V2_BOUNCE_EMPTY != engine->state[index]) {
        return -EBUSY;
    }

    engine->state[index] = RGB_V2_BOUNCE_READY;
    return 0;
}

int rgb_v2_bounce_start(struct rgb_v2_bounce_engine *engine, uint8_t first_index)
{
    uint8_t other_index = 0U;

    if ((NULL == engine) || (false == rgb_v2_valid_index(first_index))) {
        return -EINVAL;
    }

    if (true == engine->fault) {
        return -EIO;
    }

    if (RGB_V2_NO_ACTIVE_BOUNCE != engine->scanning_index) {
        return -EBUSY;
    }

    other_index = (uint8_t)(first_index ^ 1U);
    if ((RGB_V2_BOUNCE_READY != engine->state[first_index]) ||
        (RGB_V2_BOUNCE_READY != engine->state[other_index])) {
        return -EAGAIN;
    }

    engine->state[first_index] = RGB_V2_BOUNCE_SCANNING;
    engine->scanning_index = first_index;

    return 0;
}

int rgb_v2_bounce_eof(struct rgb_v2_bounce_engine *engine, uint8_t completed_index)
{
    uint8_t next_index = 0U;

    if ((NULL == engine) || (false == rgb_v2_valid_index(completed_index))) {
        return -EINVAL;
    }

    if (true == engine->fault) {
        return -EIO;
    }

    if ((completed_index != engine->scanning_index) || 
        (RGB_V2_BOUNCE_SCANNING != engine->state[completed_index])) {
        engine->fault = true;
        return -EIO;
    }

    next_index = (uint8_t)(completed_index ^ 1U);
    if (RGB_V2_BOUNCE_READY != engine->state[next_index]) {
        engine->deadline_misses++;
        engine->fault = true;
        return -ETIMEDOUT;
    }

    engine->state[completed_index] = RGB_V2_BOUNCE_EMPTY;
    engine->state[next_index] = RGB_V2_BOUNCE_SCANNING;
    engine->scanning_index = next_index;

    return 0;
}

int rgb_v2_bounce_refill_begin(struct rgb_v2_bounce_engine *engine, uint8_t index,
                               struct rgb_v2_refill_token *token)
{
    if ((NULL == engine) || (NULL == token) || (false == rgb_v2_valid_index(index))) {
        return -EINVAL;
    }

    if (true == engine->fault) {
        return -EIO;
    }

    if (RGB_V2_BOUNCE_EMPTY != engine->state[index]) {
        return -EBUSY;
    }

    engine->next_operation++;
    if (0U == engine->next_operation) {
        engine->next_operation = 1U;
    }

    engine->state[index] = RGB_V2_BOUNCE_REFILLING;
    engine->active_operation[index] = engine->next_operation;
    token->index = index;
    token->epoch = engine->epoch;
    token->operation = engine->next_operation;

    return 0;
}

int rgb_v2_bounce_refill_finish(struct rgb_v2_bounce_engine *engine,
                                const struct rgb_v2_refill_token *token, bool success)
{
    if (false == rgb_v2_valid_token(engine, token)) {
        return -EINVAL;
    }

    if (RGB_V2_BOUNCE_REFILLING != engine->state[token->index]) {
        return -EALREADY;
    }

    if (true == engine->fault) {
        return -EIO;
    }

    if (false == success) {
        engine->fault = true;
        engine->state[token->index] = RGB_V2_BOUNCE_EMPTY;
        engine->active_operation[token->index] = 0U;
        return -EIO;
    }

    engine->state[token->index] = RGB_V2_BOUNCE_READY;
    engine->active_operation[token->index] = 0U;

    return 0;
}

int rgb_v2_bounce_refill_cancel(struct rgb_v2_bounce_engine *engine,
                                const struct rgb_v2_refill_token *token)
{
    if (false == rgb_v2_valid_token(engine, token)) {
        return -EINVAL;
    }

    if (RGB_V2_BOUNCE_REFILLING != engine->state[token->index]) {
        return -EALREADY;
    }

    engine->state[token->index] = RGB_V2_BOUNCE_EMPTY;
    engine->active_operation[token->index] = 0U;
    
    return 0;
}
