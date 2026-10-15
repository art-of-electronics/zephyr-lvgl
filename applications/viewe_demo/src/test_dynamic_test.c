/*
 * ESP32-S3 RGB LCD dynamic partial-update smoke test.
 *
 * Moves the existing Samus Orange square for 30 seconds using a SINGLE
 * display_write() per step: the dirty rectangle contains both the old
 * and new square positions. No LCD driver, DMA, or framebuffer internals
 * are accessed here.
 */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>


LOG_MODULE_REGISTER(viewe_dynamic_test, LOG_LEVEL_INF);

#define DYNAMIC_SQUARE_SIZE         (100U)
#define DYNAMIC_ORANGE_RGB565       (0xFBE0U)
#define DYNAMIC_BLACK_RGB565        (0x0000U)
#define DYNAMIC_STEP_X              (11)
#define DYNAMIC_STEP_Y              (7)
#define DYNAMIC_FRAME_PERIOD_MS     (50U)
#define DYNAMIC_TEST_DURATION_MS    (30000U)
#define DYNAMIC_REPORT_PERIOD_MS    (5000U)
#define DYNAMIC_MAX_DIRTY_WIDTH     (DYNAMIC_SQUARE_SIZE + DYNAMIC_STEP_X)
#define DYNAMIC_MAX_DIRTY_HEIGHT    (DYNAMIC_SQUARE_SIZE + DYNAMIC_STEP_Y)
#define DYNAMIC_MAX_DIRTY_PIXELS    (DYNAMIC_MAX_DIRTY_WIDTH * DYNAMIC_MAX_DIRTY_HEIGHT)
#define DYNAMIC_MS_PER_SECOND       (1000U)
#define DYNAMIC_FPS_DECIMALS        (10U)

/* Static: ~23.8 KB in BSS, not on the thread's stack. */
static uint16_t dirty_pixels[DYNAMIC_MAX_DIRTY_PIXELS];

static int dynamic_draw_dirty(const struct device *display, uint16_t old_x, uint16_t old_y, uint16_t new_x, uint16_t new_y)
{
    uint16_t left = MIN(old_x, new_x);
    uint16_t top = MIN(old_y, new_y);
    uint16_t right = MAX((uint16_t)(old_x + DYNAMIC_SQUARE_SIZE), (uint16_t)(new_x + DYNAMIC_SQUARE_SIZE));
    uint16_t bottom = MAX((uint16_t)(old_y + DYNAMIC_SQUARE_SIZE), (uint16_t)(new_y + DYNAMIC_SQUARE_SIZE));
    uint16_t width = (uint16_t)(right - left);
    uint16_t height = (uint16_t)(bottom - top);
    struct display_buffer_descriptor desc = {0};
    size_t pixel_count = (size_t)width * height;

    if (pixel_count > ARRAY_SIZE(dirty_pixels)) {
        return -EINVAL;
    }

    /* Fill both the erased region and the new position before flushing. */
    for (size_t row = 0U; row < height; ++row) {
        for (size_t col = 0U; col < width; ++col) {
            uint32_t screen_x = (uint32_t)left + (uint32_t)col;
            uint32_t screen_y = (uint32_t)top + (uint32_t)row;
            bool inside = ((screen_x >= new_x) && (screen_x < ((uint32_t)new_x + DYNAMIC_SQUARE_SIZE)) &&
                           (screen_y >= new_y) && (screen_y < ((uint32_t)new_y + DYNAMIC_SQUARE_SIZE)));

            dirty_pixels[row * width + col] = (true == inside) ? (DYNAMIC_ORANGE_RGB565) : (DYNAMIC_BLACK_RGB565);
        }
    }

    desc.buf_size = pixel_count * sizeof(dirty_pixels[0]);
    desc.width = width;
    desc.height = height;
    desc.pitch = width;

    return display_write(display, left, top, &desc, dirty_pixels);
}

int dynamic_test(const struct device *display)
{
    struct display_capabilities caps = {0};
    uint16_t x = 0U;
    uint16_t y = 0U;
    uint16_t max_x = 0U;
    uint16_t max_y = 0U;
    int32_t step_x = DYNAMIC_STEP_X;
    int32_t step_y = DYNAMIC_STEP_Y;
    int64_t start_ms = 0;
    int64_t report_start_ms = 0;
    uint32_t report_frames = 0U;
    uint32_t total_frames = 0U;

    if ((NULL == display) || (false == device_is_ready(display))) {
        return -ENODEV;
    }

    display_get_capabilities(display, &caps);
    if (PIXEL_FORMAT_RGB_565 != caps.current_pixel_format) {
        LOG_ERR("RGB565 is required for this test");
        return -ENOTSUP;
    }
    if ((caps.x_resolution < DYNAMIC_SQUARE_SIZE) || (caps.y_resolution < DYNAMIC_SQUARE_SIZE)) {
        return -EINVAL;
    }

    max_x = (uint16_t)(caps.x_resolution - DYNAMIC_SQUARE_SIZE);
    max_y = (uint16_t)(caps.y_resolution - DYNAMIC_SQUARE_SIZE);
    x = (uint16_t)(max_x / 2U);
    y = (uint16_t)(max_y / 2U);

    LOG_INF("Dynamic partial-write test: 30s, target 20 updates/s, square %ux%u", 
            DYNAMIC_SQUARE_SIZE, DYNAMIC_SQUARE_SIZE);

    start_ms = k_uptime_get();
    report_start_ms = start_ms;

    while ((k_uptime_get() - start_ms) < DYNAMIC_TEST_DURATION_MS) {
        int64_t frame_start_ms = k_uptime_get();
        int64_t elapsed_ms = 0;
        int32_t next_x = (int32_t)x + step_x;
        int32_t next_y = (int32_t)y + step_y;
        int ret = 0;

        if (next_x > (int32_t)max_x) {
            next_x = (int32_t)max_x;
            step_x = -step_x;
        } else if (next_x < 0) {
            next_x = 0;
            step_x = -step_x;
        }

        if (next_y > (int32_t)max_y) {
            next_y = (int32_t)max_y;
            step_y = -step_y;
        } else if (next_y < 0) {
            next_y = 0;
            step_y = -step_y;
        }

        ret = dynamic_draw_dirty(display, x, y, (uint16_t)next_x, (uint16_t)next_y);
        if (0 != ret) {
            LOG_ERR("Partial display_write failed: %d", ret);
            return ret;
        }

        x = (uint16_t)next_x;
        y = (uint16_t)next_y;
        total_frames++;
        report_frames++;

        elapsed_ms = k_uptime_get() - report_start_ms;
        if (elapsed_ms >= DYNAMIC_REPORT_PERIOD_MS) {
            uint32_t fps_x10 = (uint32_t)(((uint64_t)report_frames * DYNAMIC_MS_PER_SECOND * DYNAMIC_FPS_DECIMALS) /
                               (uint64_t)elapsed_ms);

            LOG_INF("Partial updates: %u.%u/s, total=%u, x=%u y=%u", fps_x10 / DYNAMIC_FPS_DECIMALS,
                    fps_x10 % DYNAMIC_FPS_DECIMALS, total_frames, x, y);
            report_start_ms = k_uptime_get();
            report_frames = 0U;
        }

        elapsed_ms = k_uptime_get() - frame_start_ms;
        if (elapsed_ms < DYNAMIC_FRAME_PERIOD_MS) {
            k_sleep(K_MSEC((int32_t)(DYNAMIC_FRAME_PERIOD_MS - elapsed_ms)));
        }
    }

    LOG_INF("Dynamic test completed: %u successful partial updates", total_frames);
    return 0;
}
