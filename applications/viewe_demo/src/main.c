#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/led_strip.h>
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

#define RGB_LED_NODE 			DT_ALIAS(led_strip)
#define BACKLIGHT_NODE      	DT_ALIAS(backlight_pwm)
#define BACKLIGHT_LED_INDEX 	(0U)

#define VERSION_TEXT_SIZE		(64U)

#define ANIMATION_DURATION_MS 	(3000U)
#define ANIMATION_START_X     	(40)
#define ANIMATION_END_X       	(660)
#define ANIMATION_BOX_SIZE    	(100)

#define RUN_STRESS_TEST (0)
extern void stress_test_start(lv_obj_t *screen);

static void animation_x_cb(void *obj, int32_t value)
{
    lv_obj_set_x((lv_obj_t *)obj, value);
}

static lv_obj_t *counter_label = NULL;
static uint32_t click_count = 0U;

static void button_event_cb(lv_event_t *event)
{
    if (LV_EVENT_CLICKED == lv_event_get_code(event)) {
        char text[32] = {0};
        click_count++;
		lv_label_set_text_fmt(counter_label, "Clicks: %u", click_count);

        LOG_INF("Button clicked: %u", (unsigned int)click_count);
    }
}

static void create_ui(lv_obj_t *screen)
{
    lv_obj_t *button = NULL;
    lv_obj_t *button_label = NULL;
	
    counter_label = lv_label_create(screen);
    lv_label_set_text(counter_label, "Clicks: 0");
	lv_obj_set_style_text_font(counter_label, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(counter_label, lv_color_hex(SAMUS_PURPLE), LV_PART_MAIN);
    lv_obj_align(counter_label, LV_ALIGN_TOP_LEFT, 20, 20);
	
    button = lv_button_create(screen);
    lv_obj_set_size(button, 180, 60);
    lv_obj_align(button, LV_ALIGN_TOP_RIGHT, -20, 20);
    lv_obj_set_style_bg_color(button, lv_color_hex(SAMUS_ORANGE), LV_PART_MAIN);
    lv_obj_add_event_cb(button, button_event_cb, LV_EVENT_CLICKED, NULL);

    button_label = lv_label_create(button);
    lv_label_set_text(button_label, "CLICK ME!");
	lv_obj_set_style_text_font(button_label, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(button_label, lv_color_hex(DARK_SAMUS_ARMOR), LV_PART_MAIN);
    lv_obj_center(button_label);
	
}

int main(void)
{
	const struct device *backlight_dev = DEVICE_DT_GET(BACKLIGHT_NODE);
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	static const struct device *const rgb_led = DEVICE_DT_GET(RGB_LED_NODE);
    static lv_obj_t *screen = NULL;
	static lv_obj_t *version_label = NULL;
    static lv_obj_t *label = NULL;
	static lv_obj_t *morph_ball = NULL;

    if (true != device_is_ready(display_dev)) {
        return -ENODEV;
    }

	struct led_rgb pixel = {
		.r = 0U,
		.g = 10U,
		.b = 10U,
	};

	if (true != device_is_ready(rgb_led)) {
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

	(void)led_strip_update_rgb(rgb_led, &pixel, 1U);
	(void)display_blanking_off(display_dev);
	(void)led_set_brightness(backlight_dev, BACKLIGHT_LED_INDEX, 35);

	lv_display_t *lv_display = lv_display_get_default();
	if (NULL == lv_display) {
    	LOG_ERR("LVGL display not available");
    	return -ENODEV;
	}

    //lv_obj_set_style_bg_color(screen, lv_color_hex(DARK_SAMUS_ARMOR), LV_PART_MAIN);
    //lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

	lv_obj_set_style_bg_color(screen, lv_color_hex(DARK_SAMUS_BLUE), LV_PART_MAIN);
	lv_obj_set_style_bg_grad_color(screen, lv_color_hex(DARK_SAMUS_ARMOR),LV_PART_MAIN);
	lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, LV_PART_MAIN);
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
	lv_obj_set_style_text_color(label, lv_color_hex(SAMUS_RED), LV_PART_MAIN);
	lv_obj_align_to(label, version_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 150);

	morph_ball = lv_obj_create(screen);
	lv_obj_set_size(morph_ball, ANIMATION_BOX_SIZE, ANIMATION_BOX_SIZE);
	lv_obj_set_style_bg_color(morph_ball, lv_color_hex(SAMUS_ORANGE), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(morph_ball, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_radius(morph_ball, (ANIMATION_BOX_SIZE / 2U), LV_PART_MAIN);
	lv_obj_set_style_border_width(morph_ball, 0, LV_PART_MAIN);
	lv_obj_set_pos(morph_ball, ANIMATION_START_X, 280);

	static lv_anim_t anim;
	lv_anim_init(&anim);
	lv_anim_set_var(&anim, morph_ball);
	lv_anim_set_exec_cb(&anim, animation_x_cb);
	lv_anim_set_values(&anim, ANIMATION_START_X, ANIMATION_END_X);
	lv_anim_set_duration(&anim, ANIMATION_DURATION_MS);
	lv_anim_set_playback_duration(&anim, ANIMATION_DURATION_MS);
	lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
	lv_anim_start(&anim);

	create_ui(screen);

#if RUN_STRESS_TEST
	stress_test_start(screen);
#endif

	uint32_t start_cycles = 0U;
	uint32_t elapsed_cycles = 0U;
	uint32_t elapsed_us = 0U;

	uint64_t total_us = 0U;
	uint32_t max_us = 0U;
	uint32_t samples = 0U;
	uint32_t slow_calls = 0U;
	int64_t last_log_ms = k_uptime_get();

	while (true) {
    	start_cycles = k_cycle_get_32();

    	lv_timer_handler();

    	elapsed_cycles = k_cycle_get_32() - start_cycles;
    	elapsed_us = k_cyc_to_us_floor32(elapsed_cycles);

    	total_us += elapsed_us;
    	max_us = MAX(max_us, elapsed_us);
    	samples++;

    	if (20000U <= elapsed_us) {
        	slow_calls++;
    	}

    	if (1000 <= (k_uptime_get() - last_log_ms)) {
        	LOG_INF("LVGL: avg=%llu us, max=%u us, slow=%u/%u",
                (unsigned long long)(total_us / samples),
                (unsigned int)max_us,
                (unsigned int)slow_calls,
                (unsigned int)samples);

        	total_us = 0U;
        	max_us = 0U;
        	samples = 0U;
        	slow_calls = 0U;
        	last_log_ms = k_uptime_get();
    	}

    	k_sleep(K_MSEC(10));
	}

    return 0;
}
