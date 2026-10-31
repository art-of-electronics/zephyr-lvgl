#include <demo_lcd.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/mipi_dbi.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo_lcd, LOG_LEVEL_INF);

#define LCD_BACKLIGHT_NODE  DT_ALIAS(backlight)
#define LCD_NODE            DT_CHOSEN(zephyr_display)

#define PWM_BACKLIGHT_MIN   (10U)
#define PWM_MAX_BRIGHTNESS  (100U)

#define LCD_WIDTH           (320U)
#define LCD_HEIGHT          (480U)
#define LCD_BUFFER_ROWS     (8U)

#define LCD_CMD_COLMOD      (0x3AU)
#define LCD_CMD_MADCTL      (0x36U)
#define LCD_COLMOD_RGB565   (0x55U)
#define LCD_MADCTL_VALUE    (0x48U)

#define SAMUS_ORANGE        (0xF465U)
#define SAMUS_RED           (0xD145U)
#define SAMUS_BLUE          (0x245BU)
#define SAMUS_GREEN         (0x564FU)
#define SAMUS_PURPLE        (0x8238U)
#define DARK_SAMUS_BLUE     (0x067FU)
#define DARK_SAMUS_ARMOR    (0x114BU)

#define LCD_RED     (0xF800U)
#define LCD_GREEN   (0x07E0U)
#define LCD_BLUE    (0x001FU)
#define LCD_WHITE   (0xFFFFU)
#define LCD_BLACK   (0x0000U)

static uint16_t lcd_buffer[LCD_WIDTH * LCD_BUFFER_ROWS];

static const struct device *const lcd_dbi = DEVICE_DT_GET(DT_PARENT(LCD_NODE));
static const struct pwm_dt_spec lcd_backlight = PWM_DT_SPEC_GET(LCD_BACKLIGHT_NODE);
static const struct mipi_dbi_config lcd_dbi_config = MIPI_DBI_CONFIG_DT(LCD_NODE, SPI_OP_MODE_CONTROLLER | SPI_WORD_SET(8), 0);
static const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static int pwm_set_brightness(const struct pwm_dt_spec *pwm, uint32_t brightness)
{
    uint64_t pulse = 0U;

    if (PWM_MAX_BRIGHTNESS < brightness) {
        return -EINVAL;
    }

    pulse = ((uint64_t)pwm->period * brightness) / PWM_MAX_BRIGHTNESS;

    return pwm_set_dt(pwm, pwm->period, (uint32_t)pulse);
}

static int lcd_configure_test(void)
{
    const uint8_t colmod = LCD_COLMOD_RGB565;
    const uint8_t madctl = LCD_MADCTL_VALUE;
    int ret = 0;

    if (true != device_is_ready(lcd_dbi)) {
        return -ENODEV;
    }

    ret = mipi_dbi_command_write(
        lcd_dbi,
        &lcd_dbi_config,
        LCD_CMD_COLMOD,
        &colmod,
        sizeof(colmod)
    );

    if (0 != ret) {
        return ret;
    }

    ret = mipi_dbi_command_write(
        lcd_dbi,
        &lcd_dbi_config,
        LCD_CMD_MADCTL,
        &madctl,
        sizeof(madctl)
    );

    return ret;
}

static int lcd_fill(const struct device *display, uint16_t color)
{
    struct display_buffer_descriptor desc = {0};
    uint32_t i = 0U;
    uint16_t y = 0U;
    int ret = 0;

    for (i = 0U; i < ARRAY_SIZE(lcd_buffer); i++) {
        lcd_buffer[i] = sys_cpu_to_be16(color);
    }

    desc.width = LCD_WIDTH;
    desc.height = LCD_BUFFER_ROWS;
    desc.pitch = LCD_WIDTH;
    desc.buf_size = sizeof(lcd_buffer);

    for (y = 0U; y < LCD_HEIGHT; y += LCD_BUFFER_ROWS) {
        ret = display_write(display, 0U, y, &desc, lcd_buffer);
        if (0 != ret) {
            return ret;
        }
    }

    return 0;
}

int demo_lcd_pwm_init(void)
{
    if (true != pwm_is_ready_dt(&lcd_backlight)) {
        LOG_ERR("PWM controller not ready");
        return -ENODEV;
    }

    return 0;
}

int demo_lcd_set_brightness(uint32_t brightness) 
{
    int ret = pwm_set_brightness(&lcd_backlight, brightness);
    if (0 != ret) {
        LOG_ERR("Backlight PWM failed: %d", ret);
        return ret;
    }

    return 0;
}

int demo_lcd_init_lcd(void)
{
    int ret = 0;

    struct display_capabilities capabilities = {0};
    if (true != device_is_ready(display)) {
        LOG_ERR("Display device not ready");
        return -ENODEV;
    }

    display_get_capabilities(display, &capabilities);

    LOG_INF("Display device: %s", display->name);
    LOG_INF("Display: %u x %u", capabilities.x_resolution, capabilities.y_resolution);
    LOG_INF("Pixel format: 0x%x", capabilities.current_pixel_format);

    if (PIXEL_FORMAT_RGB_565 != capabilities.current_pixel_format) {
        ret = display_set_pixel_format(display, PIXEL_FORMAT_RGB_565);
        if (0 != ret) {
            LOG_ERR("Cannot set RGB565: %d", ret);
            return ret;
        }
    }

    ret = display_blanking_off(display);
    if (0 != ret) {
        LOG_ERR("Display blanking off failed: %d", ret);
        return ret;
    }

    ret = lcd_configure_test();
    if (0 != ret) {
        LOG_ERR("LCD configuration failed: %d", ret);
        return ret;
    }

    return 0;
}

void demo_lcd_loop(void) 
{
    int ret = 0;

    ret = lcd_fill(display, LCD_RED);
    if (0 != ret) {
        LOG_ERR("Display write failed: %d", ret);
        return;
    }
    k_msleep(1000);

    ret = lcd_fill(display, LCD_GREEN);
    if (0 != ret) {
        return;
    }
    k_msleep(1000);

    ret = lcd_fill(display, LCD_BLUE);
    if (0 != ret) {
        return;
    }
    k_msleep(1000);

    ret = lcd_fill(display, LCD_WHITE);
    if (0 != ret) {
        return;
    }
    k_msleep(1000);

    ret = lcd_fill(display, LCD_BLACK);
    if (0 != ret) {
        return;
    }
    k_msleep(1000);
}
