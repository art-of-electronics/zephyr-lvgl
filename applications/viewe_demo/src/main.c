#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/display.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/version.h>

#include <stdint.h>
#include <lvgl.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define LVGL_TASK_DELAY_MS  (10U)

#define SAMUS_ORANGE		(0xF28C28U)
#define SAMUS_RED       	(0xD62828U)
#define SAMUS_BLUE      	(0x2389DAU)
#define SAMUS_GREEN     	(0x50C878U)
#define SAMUS_PURPLE    	(0x8445C7U)
#define DARK_SAMUS_BLUE     (0x00CFFFU)
#define DARK_SAMUS_ARMOR	(0x142B5AU)

#define BACKLIGHT_NODE       DT_ALIAS(backlight_pwm)
#define BACKLIGHT_LED_INDEX  (0U)

#define VERSION_TEXT_SIZE (64U)

int main(void)
{
	const struct device *backlight_dev = DEVICE_DT_GET(BACKLIGHT_NODE);
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    lv_obj_t *screen = NULL;
	lv_obj_t *version_label = NULL;
    lv_obj_t *label = NULL;

    if (true != device_is_ready(display_dev)) {
        return -ENODEV;
    }

	if (true != device_is_ready(backlight_dev)) {
    	LOG_ERR("Backlight device not ready");
    	return -ENODEV;
	}

	screen = lv_screen_active();
	if (NULL == screen) {
    	LOG_ERR("LVGL active screen is NULL");
    	return -ENODEV;
	}

	LOG_INF("LCD RGB started");

	(void)display_blanking_off(display_dev);
	(void)led_set_brightness(backlight_dev, BACKLIGHT_LED_INDEX, 35);

	lv_display_t *lv_display = lv_display_get_default();
	if (NULL == lv_display) {
    	LOG_ERR("LVGL display not available");
    	return -ENODEV;
	}

    lv_obj_set_style_bg_color(screen, lv_color_hex(DARK_SAMUS_ARMOR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

	char version_text[VERSION_TEXT_SIZE] = {0};
	(void)snprintf(version_text, sizeof(version_text),
 				   "Zephyr %u.%u x LVGL %u.%u", 
				   (unsigned int)KERNEL_VERSION_MAJOR, (unsigned int)KERNEL_VERSION_MINOR,
				   (unsigned int)lv_version_major(), (unsigned int)lv_version_minor());

    version_label = lv_label_create(screen);
    lv_label_set_text(version_label, version_text);
	lv_obj_set_style_text_font(version_label, &lv_font_montserrat_48, LV_PART_MAIN);
	lv_obj_set_style_text_color(version_label, lv_color_hex(SAMUS_ORANGE), LV_PART_MAIN);
	lv_obj_center(version_label);

	label = lv_label_create(screen);
	lv_label_set_text(label, "@_art_of_electronics_");
	lv_obj_set_style_text_font(label, &lv_font_montserrat_28, LV_PART_MAIN);
	lv_obj_set_style_text_color(label, lv_color_hex(SAMUS_PURPLE), LV_PART_MAIN);
	lv_obj_align_to(label, version_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 12);

    while (true) {
    	lv_timer_handler();
    	k_sleep(K_MSEC(LVGL_TASK_DELAY_MS));
    }

    return 0;
}
