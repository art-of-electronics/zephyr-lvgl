#define DT_DRV_COMPAT espressif_esp32_lcd_cam_rgb

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(esp32_lcd_cam_rgb, CONFIG_DISPLAY_LOG_LEVEL);

struct esp32_lcd_cam_rgb_config {
    uint16_t width;
    uint16_t height;
    uint32_t pixel_clock;
};

struct esp32_lcd_cam_rgb_data {
    enum display_pixel_format pixel_format;
    enum display_orientation orientation;
};

static int esp32_lcd_cam_rgb_write(
    const struct device *dev,
    const uint16_t x,
    const uint16_t y,
    const struct display_buffer_descriptor *desc,
    const void *buf)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(x);
    ARG_UNUSED(y);
    ARG_UNUSED(desc);
    ARG_UNUSED(buf);

    return -ENOTSUP;
}

static void esp32_lcd_cam_rgb_get_capabilities(
    const struct device *dev,
    struct display_capabilities *capabilities)
{
    const struct esp32_lcd_cam_rgb_config *config = dev->config;
    const struct esp32_lcd_cam_rgb_data *data = dev->data;

    memset(capabilities, 0, sizeof(*capabilities));

    capabilities->x_resolution = config->width;
    capabilities->y_resolution = config->height;

    capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_565;
    capabilities->current_pixel_format = data->pixel_format;
    capabilities->current_orientation = data->orientation;
}

static int esp32_lcd_cam_rgb_set_pixel_format(
    const struct device *dev,
    const enum display_pixel_format pixel_format)
{
    struct esp32_lcd_cam_rgb_data *data = dev->data;

    if (pixel_format != PIXEL_FORMAT_RGB_565) {
        return -ENOTSUP;
    }

    data->pixel_format = pixel_format;

    return 0;
}

static int esp32_lcd_cam_rgb_set_orientation(
    const struct device *dev,
    const enum display_orientation orientation)
{
    struct esp32_lcd_cam_rgb_data *data = dev->data;

    if (orientation != DISPLAY_ORIENTATION_NORMAL) {
        return -ENOTSUP;
    }

    data->orientation = orientation;

    return 0;
}

static int esp32_lcd_cam_rgb_init(const struct device *dev)
{
    const struct esp32_lcd_cam_rgb_config *config = dev->config;
    struct esp32_lcd_cam_rgb_data *data = dev->data;

    data->pixel_format = PIXEL_FORMAT_RGB_565;
    data->orientation = DISPLAY_ORIENTATION_NORMAL;

    LOG_INF("ESP32 LCD_CAM RGB display initialized");
    LOG_INF("Resolution: %u x %u", config->width, config->height);
    LOG_INF("Pixel clock: %u Hz", config->pixel_clock);

    return 0;
}

static DEVICE_API(display, esp32_lcd_cam_rgb_api) = {
    .write = esp32_lcd_cam_rgb_write,
    .get_capabilities = esp32_lcd_cam_rgb_get_capabilities,
    .set_pixel_format = esp32_lcd_cam_rgb_set_pixel_format,
    .set_orientation = esp32_lcd_cam_rgb_set_orientation,
};

#define ESP32_LCD_CAM_RGB_DEFINE(inst)                                      \
    static const struct esp32_lcd_cam_rgb_config config_##inst = {         \
        .width = DT_INST_PROP(inst, width),                                \
        .height = DT_INST_PROP(inst, height),                              \
        .pixel_clock = DT_PROP(                                           \
            DT_INST_CHILD(inst, display_timings), clock_frequency),       \
    };                                                                     \
                                                                           \
    static struct esp32_lcd_cam_rgb_data data_##inst;                     \
                                                                           \
    DEVICE_DT_INST_DEFINE(inst,                                            \
                          esp32_lcd_cam_rgb_init,                          \
                          NULL,                                            \
                          &data_##inst,                                    \
                          &config_##inst,                                  \
                          POST_KERNEL,                                     \
                          CONFIG_DISPLAY_INIT_PRIORITY,                   \
                          &esp32_lcd_cam_rgb_api);

DT_INST_FOREACH_STATUS_OKAY(ESP32_LCD_CAM_RGB_DEFINE)
