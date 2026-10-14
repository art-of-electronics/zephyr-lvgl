/*
 * VIEWE ESP32-S3 RGB565 display throughput benchmark.
 *
 * One partial display_write() per update. No FPS cap, no sleep in the loop.
 * Tests dirty-rectangle updates without touching LCD/GDMA driver internals.
 * The application remains responsible for painting the starting square.
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

#define DYNAMIC_SQUARE_SIZE          (100U)
#define DYNAMIC_ORANGE_RGB565        (0xFBE0U)
#define DYNAMIC_BLACK_RGB565         (0x0000U)
#define DYNAMIC_STEP_X              (11)
#define DYNAMIC_STEP_Y              (7)
#define DYNAMIC_TEST_DURATION_MS     (30000U)
#define DYNAMIC_REPORT_PERIOD_MS     (5000U)
#define DYNAMIC_MAX_DIRTY_WIDTH      (DYNAMIC_SQUARE_SIZE + DYNAMIC_STEP_X)
#define DYNAMIC_MAX_DIRTY_HEIGHT     (DYNAMIC_SQUARE_SIZE + DYNAMIC_STEP_Y)
#define DYNAMIC_MAX_DIRTY_PIXELS     (DYNAMIC_MAX_DIRTY_WIDTH * DYNAMIC_MAX_DIRTY_HEIGHT)
#define DYNAMIC_MS_PER_SECOND        (1000U)
#define DYNAMIC_FPS_DECIMALS         (10U)

/* 111 x 107 x 2 = 23754 bytes in BSS, not on the thread stack. */
static uint16_t dirty_pixels[DYNAMIC_MAX_DIRTY_PIXELS];

static void dynamic_move_axis(uint16_t current, int32_t *step,
                              uint16_t maximum, uint16_t *next)
{
    int32_t position = (int32_t)current + *step;

    /* Reflect overshoot rather than clamping at the edge. */
    if (position > (int32_t)maximum) {
        position = (2 * (int32_t)maximum) - position;
        *step = -*step;
    } else if (position < 0) {
        position = -position;
        *step = -*step;
    }

    *next = (uint16_t)position;
}

static int dynamic_draw_dirty(const struct device *display,
                              uint16_t old_x, uint16_t old_y,
                              uint16_t new_x, uint16_t new_y,
                              uint32_t *write_us)
{
    uint16_t left = MIN(old_x, new_x);
    uint16_t top = MIN(old_y, new_y);
    uint16_t right = MAX((uint16_t)(old_x + DYNAMIC_SQUARE_SIZE),
                         (uint16_t)(new_x + DYNAMIC_SQUARE_SIZE));
    uint16_t bottom = MAX((uint16_t)(old_y + DYNAMIC_SQUARE_SIZE),
                          (uint16_t)(new_y + DYNAMIC_SQUARE_SIZE));
    uint16_t width = (uint16_t)(right - left);
    uint16_t height = (uint16_t)(bottom - top);
    size_t pixel_count = (size_t)width * height;
    struct display_buffer_descriptor desc = {0};
    uint32_t before_write = 0U;
    uint32_t after_write = 0U;
    int ret = 0;

    if (pixel_count > ARRAY_SIZE(dirty_pixels)) {
        return -EINVAL;
    }

    for (size_t row = 0U; row < height; ++row) {
        for (size_t col = 0U; col < width; ++col) {
            uint32_t screen_x = (uint32_t)left + (uint32_t)col;
            uint32_t screen_y = (uint32_t)top + (uint32_t)row;
            bool inside = ((screen_x >= new_x) &&
                           (screen_x < ((uint32_t)new_x + DYNAMIC_SQUARE_SIZE)) &&
                           (screen_y >= new_y) &&
                           (screen_y < ((uint32_t)new_y + DYNAMIC_SQUARE_SIZE)));

            dirty_pixels[row * width + col] =
                (true == inside) ? DYNAMIC_ORANGE_RGB565 : DYNAMIC_BLACK_RGB565;
        }
    }

    desc.buf_size = pixel_count * sizeof(dirty_pixels[0]);
    desc.width = width;
    desc.height = height;
    desc.pitch = width;

    before_write = k_cycle_get_32();
    ret = display_write(display, left, top, &desc, dirty_pixels);
    after_write = k_cycle_get_32();
    *write_us = k_cyc_to_us_floor32(after_write - before_write);

    return ret;
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
    uint32_t total_frames = 0U;
    uint32_t report_frames = 0U;
    uint64_t report_write_us = 0U;
    uint64_t report_frame_us = 0U;
    uint32_t max_write_us = 0U;

    if ((NULL == display) || (false == device_is_ready(display))) {
        return -ENODEV;
    }

    display_get_capabilities(display, &caps);
    if (PIXEL_FORMAT_RGB_565 != caps.current_pixel_format) {
        LOG_ERR("RGB565 is required for this test");
        return -ENOTSUP;
    }
    if ((caps.x_resolution < DYNAMIC_SQUARE_SIZE) ||
        (caps.y_resolution < DYNAMIC_SQUARE_SIZE)) {
        return -EINVAL;
    }

    max_x = (uint16_t)(caps.x_resolution - DYNAMIC_SQUARE_SIZE);
    max_y = (uint16_t)(caps.y_resolution - DYNAMIC_SQUARE_SIZE);
    x = (uint16_t)(max_x / 2U);
    y = (uint16_t)(max_y / 2U);

    LOG_INF("Uncapped RGB565 partial-write benchmark: %u s, square %ux%u",
            DYNAMIC_TEST_DURATION_MS / DYNAMIC_MS_PER_SECOND,
            DYNAMIC_SQUARE_SIZE, DYNAMIC_SQUARE_SIZE);

    start_ms = k_uptime_get();
    report_start_ms = start_ms;

    while ((k_uptime_get() - start_ms) < DYNAMIC_TEST_DURATION_MS) {
        uint16_t next_x = 0U;
        uint16_t next_y = 0U;
        uint32_t write_us = 0U;
        uint32_t frame_start_cycles = k_cycle_get_32();
        uint32_t frame_us = 0U;
        int64_t report_elapsed_ms = 0;
        int ret = 0;

        dynamic_move_axis(x, &step_x, max_x, &next_x);
        dynamic_move_axis(y, &step_y, max_y, &next_y);

        ret = dynamic_draw_dirty(display, x, y, next_x, next_y, &write_us);
        if (0 != ret) {
            LOG_ERR("Partial display_write failed: %d", ret);
            return ret;
        }

        frame_us = k_cyc_to_us_floor32(k_cycle_get_32() - frame_start_cycles);
        x = next_x;
        y = next_y;
        total_frames++;
        report_frames++;
        report_write_us += write_us;
        report_frame_us += frame_us;
        max_write_us = MAX(max_write_us, write_us);

        report_elapsed_ms = k_uptime_get() - report_start_ms;
        if ((report_elapsed_ms >= DYNAMIC_REPORT_PERIOD_MS) && (report_frames > 0U)) {
            uint32_t fps_x10 = (uint32_t)(((uint64_t)report_frames *
                DYNAMIC_MS_PER_SECOND * DYNAMIC_FPS_DECIMALS) /
                (uint64_t)report_elapsed_ms);
            uint32_t mean_write_us = (uint32_t)(report_write_us / report_frames);
            uint32_t mean_frame_us = (uint32_t)(report_frame_us / report_frames);

            LOG_INF("Updates %u.%u/s, total=%u, write avg/max=%u/%u us, "
                    "frame avg=%u us, x=%u y=%u",
                    fps_x10 / DYNAMIC_FPS_DECIMALS,
                    fps_x10 % DYNAMIC_FPS_DECIMALS,
                    total_frames, mean_write_us, max_write_us,
                    mean_frame_us, x, y);

            report_start_ms = k_uptime_get();
            report_frames = 0U;
            report_write_us = 0U;
            report_frame_us = 0U;
            max_write_us = 0U;
        }
    }

    {
        int64_t elapsed_ms = k_uptime_get() - start_ms;
        uint32_t fps_x10 = 0U;

        if (elapsed_ms > 0) {
            fps_x10 = (uint32_t)(((uint64_t)total_frames *
                DYNAMIC_MS_PER_SECOND * DYNAMIC_FPS_DECIMALS) /
                (uint64_t)elapsed_ms);
        }
        LOG_INF("Uncapped test complete: %u updates in %lld ms, avg=%u.%u/s",
                total_frames, (long long)elapsed_ms,
                fps_x10 / DYNAMIC_FPS_DECIMALS,
                fps_x10 % DYNAMIC_FPS_DECIMALS);
    }

    return 0;
}
