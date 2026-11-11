#include <errno.h>
#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <zephyr/version.h>

#include <lvgl.h>

#include <backlight.h>
#include <display.h>
#include <theme.h>

LOG_MODULE_REGISTER(app_display, LOG_LEVEL_INF);

#define VERSION_TEXT_SIZE     (64U)
#define VERSION_LABEL_Y_POS   (12)

static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

int app_display_init(lv_color_t color_bg)
{
    lv_display_t *display = NULL;
    lv_obj_t *screen = NULL;

    if (false == device_is_ready(display_dev)) {
        LOG_ERR("Display device not ready");
        return -ENODEV;
    }

    display = lv_display_get_default();
    if (NULL == display) {
        LOG_ERR("No LVGL display");
        return -ENODEV;
    }

    screen = lv_screen_active();
    if (NULL == screen) {
        LOG_ERR("No active LVGL screen");
        return -ENODEV;
    }

    lv_obj_set_style_bg_color(screen, color_bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_scrollable(screen, false);

    return 0;
}

int app_display_enable(void)
{
    int ret = 0;

    ret = display_blanking_off(display_dev);
    if ((0 > ret) && (-ENOSYS != ret)) {
        LOG_ERR("Failed to disable display blanking: %d", ret);
        return ret;
    }

    ret = backlight_init();
    if (0 != ret) {
        LOG_WRN("Backlight initialization failed: %d", ret);
    }

    return 0;
}

void app_display_create_version_label(lv_obj_t *screen, lv_color_t text_color)
{
    if (NULL == screen) {
        return;
    }

    lv_obj_t *version_label = NULL;
    char version_text[VERSION_TEXT_SIZE] = {0};

    (void)snprintf(version_text, sizeof(version_text),
                   "Zephyr %u.%u x LVGL %u.%u",
                   (unsigned int)KERNEL_VERSION_MAJOR, (unsigned int)KERNEL_VERSION_MINOR,
                   (unsigned int)lv_version_major(), (unsigned int)lv_version_minor());
    version_label = lv_label_create(screen);
    if (NULL == version_label) {
        LOG_ERR("Failed to create version label");
        return;
    }

    lv_label_set_text(version_label, version_text);
    lv_obj_set_style_text_font(version_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(version_label, text_color, LV_PART_MAIN);
    lv_obj_set_style_text_align(version_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(version_label, LV_ALIGN_TOP_MID, 0, VERSION_LABEL_Y_POS);
}
