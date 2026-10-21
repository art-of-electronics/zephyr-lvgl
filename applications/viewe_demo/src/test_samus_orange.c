/* Optional smoke-test helper: keep test patterns outside the display driver. */
#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/sys/util.h>

#define SAMUS_SQUARE_SIZE       (100U)
#define SAMUS_ORANGE_RGB565     (0xFBE0U)

int samus_orange_smoke_test(const struct device *display)
{
    static uint16_t pixels[SAMUS_SQUARE_SIZE * SAMUS_SQUARE_SIZE];
    struct display_capabilities caps = {0};
    struct display_buffer_descriptor desc = {
        .buf_size = sizeof(pixels),
        .width = SAMUS_SQUARE_SIZE,
        .height = SAMUS_SQUARE_SIZE,
        .pitch = SAMUS_SQUARE_SIZE,
    };
    uint16_t x = 0U;
    uint16_t y = 0U;

    if ((NULL == display) || (!device_is_ready(display))) {
        return -ENODEV;
    }

    display_get_capabilities(display, &caps);
    if ((caps.x_resolution < SAMUS_SQUARE_SIZE) || (caps.y_resolution < SAMUS_SQUARE_SIZE)) {
        return -EINVAL;
    }

    x = (uint16_t)((caps.x_resolution - SAMUS_SQUARE_SIZE) / 2U);
    y = (uint16_t)((caps.y_resolution - SAMUS_SQUARE_SIZE) / 2U);

    for (size_t i = 0U; i < ARRAY_SIZE(pixels); ++i) {
        pixels[i] = SAMUS_ORANGE_RGB565;
    }

    return display_write(display, x, y, &desc, pixels);
}
