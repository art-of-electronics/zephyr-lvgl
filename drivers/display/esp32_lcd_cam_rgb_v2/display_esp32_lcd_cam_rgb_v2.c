/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>

#include "rgb_v2_scanout.h"
#include "rgb_v2_rotation.h"
#include "rgb_v2_display_control.h"


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
LOG_MODULE_REGISTER(esp32_lcd_rgb_v2, CONFIG_DISPLAY_LOG_LEVEL);

#define DT_DRV_COMPAT espressif_esp32_lcd_cam_rgb

#if defined(CONFIG_ESP32_LCD_RGB_ROTATION_90)
#define RGB_V2_INITIAL_ORIENTATION DISPLAY_ORIENTATION_ROTATED_90
#elif defined(CONFIG_ESP32_LCD_RGB_ROTATION_180)
#define RGB_V2_INITIAL_ORIENTATION DISPLAY_ORIENTATION_ROTATED_180
#elif defined(CONFIG_ESP32_LCD_RGB_ROTATION_270)
#define RGB_V2_INITIAL_ORIENTATION DISPLAY_ORIENTATION_ROTATED_270
#else
#define RGB_V2_INITIAL_ORIENTATION DISPLAY_ORIENTATION_NORMAL
#endif


/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/
struct rgb_v2_display_data {
    struct rgb_v2_scanout scanout;
    enum display_orientation orientation;
};

static struct rgb_v2_display_data display_data __aligned(64);


/*****************************************************************************
                         PRIVATE FUNCTION DECLARATIONS
*****************************************************************************/

/*****************************************************************************
                       PRIVATE FUNCTION IMPLEMENTATIONS
*****************************************************************************/
static enum rgb_v2_rotation rgb_v2_orientation_to_rotation(enum display_orientation orientation)
{
    switch (orientation) {
    case DISPLAY_ORIENTATION_ROTATED_90:
        return RGB_V2_ROTATION_90;
    case DISPLAY_ORIENTATION_ROTATED_180:
        return RGB_V2_ROTATION_180;
    case DISPLAY_ORIENTATION_ROTATED_270:
        return RGB_V2_ROTATION_270;
    default:
        return RGB_V2_ROTATION_0;
    }
}

static int rgb_v2_display_init(const struct device *dev)
{
    struct rgb_v2_display_data *data = dev->data;

    data->orientation = RGB_V2_INITIAL_ORIENTATION;
    LOG_INF("Viewe RGB565 display (%ux%u)",
            (unsigned int)RGB_V2_WIDTH, (unsigned int)RGB_V2_HEIGHT);

    return rgb_v2_scanout_init(&data->scanout);
}

static int rgb_v2_display_write(const struct device *dev, uint16_t x, uint16_t y,
                                const struct display_buffer_descriptor *desc, const void *buf)
{
    struct rgb_v2_display_data *data = dev->data;

    return rgb_v2_scanout_write(&data->scanout, rgb_v2_orientation_to_rotation(data->orientation),
                                x, y, desc, buf);
}

static void rgb_v2_display_get_capabilities(const struct device *dev,
                                            struct display_capabilities *capabilities)
{
    const struct rgb_v2_display_data *data = dev->data;

    memset(capabilities, 0, sizeof(*capabilities));
    rgb_v2_rotation_dimensions(RGB_V2_WIDTH, RGB_V2_HEIGHT,
                               rgb_v2_orientation_to_rotation(data->orientation),
                               &capabilities->x_resolution, &capabilities->y_resolution);
    capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_565X;
    capabilities->current_pixel_format = PIXEL_FORMAT_RGB_565X;
    capabilities->current_orientation = data->orientation;
}

static int rgb_v2_display_set_pixel_format(const struct device *dev,
                                           const enum display_pixel_format pixel_format)
{
    ARG_UNUSED(dev);
    return (PIXEL_FORMAT_RGB_565X == pixel_format) ? (0) : (-ENOTSUP);
}

static int rgb_v2_display_set_orientation(const struct device *dev,
                                           const enum display_orientation orientation)
{
    struct rgb_v2_display_data *data = dev->data;

    if (RGB_V2_INITIAL_ORIENTATION != orientation) {
        return -ENOTSUP;
    }
    data->orientation = orientation;

    return 0;
}

static DEVICE_API(display, rgb_v2_display_api) = {
    .write = rgb_v2_display_write,
    .get_capabilities = rgb_v2_display_get_capabilities,
    .set_pixel_format = rgb_v2_display_set_pixel_format,
    .set_orientation = rgb_v2_display_set_orientation,
};


/*****************************************************************************
                           INTERFACE IMPLEMENTATION
*****************************************************************************/
DEVICE_DT_INST_DEFINE(0, rgb_v2_display_init, NULL, &display_data, NULL,
                      POST_KERNEL, CONFIG_DISPLAY_INIT_PRIORITY, &rgb_v2_display_api);

int rgb_v2_display_acquire(const struct device *dev, uint8_t *index, uint16_t **pixels)
{
    struct rgb_v2_display_data *data = NULL;
    if ((NULL == dev) || (NULL == dev->data)) {
        return -EINVAL;
    }

    data = dev->data;
    return rgb_v2_scanout_acquire(&data->scanout, index, pixels);
}

int rgb_v2_display_submit(const struct device *dev, uint8_t index)
{
    struct rgb_v2_display_data *data = NULL;
    if ((NULL == dev) || (NULL == dev->data)) {
        return -EINVAL;
    }

    data = dev->data;
    return rgb_v2_scanout_submit(&data->scanout, index);
}

int rgb_v2_display_cancel(const struct device *dev, uint8_t index)
{
    struct rgb_v2_display_data *data = NULL;
    if ((NULL == dev) || (NULL == dev->data)) {
        return -EINVAL;
    }

    data = dev->data;
    return rgb_v2_scanout_cancel(&data->scanout, index);
}
