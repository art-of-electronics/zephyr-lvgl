#include <demo_lcd.h>
#include <cyd_st7796s.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/display.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>
#include <zephyr/version.h>

#include <lvgl.h>

LOG_MODULE_REGISTER(demo_lcd, LOG_LEVEL_INF);

#define LCD_BACKLIGHT_NODE  DT_ALIAS(backlight)

#define PWM_BACKLIGHT_MIN   (10U)
#define PWM_MAX_BRIGHTNESS  (100U)

#define LVGL_TASK_DELAY_MS  (10U)

#define VERSION_TEXT_SIZE	    (64U)
#define ANIMATION_DURATION_MS 	(3000U)
#define ANIMATION_BOX_SIZE    	(60)

#define SAMUS_ORANGE		(0xF28C28U)
#define SAMUS_RED       	(0xD62828U)
#define SAMUS_BLUE      	(0x2389DAU)
#define SAMUS_GREEN     	(0x50C878U)
#define SAMUS_PURPLE    	(0x8445C7U)
#define DARK_SAMUS_BLUE     (0x00CFFFU)
#define DARK_SAMUS_ARMOR	(0x142B5AU)

static lv_obj_t *screen = NULL;
static lv_obj_t *counter_label = NULL;
static uint32_t click_count = 0U;

static const struct pwm_dt_spec lcd_backlight = PWM_DT_SPEC_GET(LCD_BACKLIGHT_NODE);
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

static void clickme_button_event_cb(lv_event_t *event)
{
    if (LV_EVENT_CLICKED == lv_event_get_code(event)) {
        click_count++;
		lv_label_set_text_fmt(counter_label, "Clicks: %u", click_count);

        LOG_INF("Button clicked: %u", (unsigned int)click_count);
    }
}

static void create_clickme(lv_obj_t *screen)
{
    lv_obj_t *button = NULL;
    lv_obj_t *button_label = NULL;

	lv_display_t *display = lv_obj_get_display(screen);
	int32_t screen_width = lv_display_get_horizontal_resolution(display);
	int32_t screen_height = lv_display_get_vertical_resolution(display);
	
    counter_label = lv_label_create(screen);
    lv_label_set_text(counter_label, "Clicks: 0");
	lv_obj_set_style_text_font(counter_label, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(counter_label, lv_color_hex(SAMUS_PURPLE), LV_PART_MAIN);
    lv_obj_align(counter_label, LV_ALIGN_TOP_LEFT, 20, 20);

	button = lv_obj_create(screen);
	lv_obj_set_size(button, 160, 50);
	if (screen_height > screen_width) {
		lv_obj_align(button, LV_ALIGN_TOP_RIGHT, -20, 60);
	} else {
		lv_obj_align(button, LV_ALIGN_TOP_RIGHT, -20, 20);
	}
	lv_obj_set_scrollable(button, false);
	lv_obj_set_clickable(button, true);

	lv_obj_set_style_bg_color(button, lv_color_hex(SAMUS_ORANGE), LV_PART_MAIN);
	lv_obj_set_style_bg_color(button, lv_color_hex(DARK_SAMUS_ARMOR), LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
	lv_obj_set_style_radius(button, 10, LV_PART_MAIN);
	lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);

	lv_obj_set_style_text_color(button, lv_color_hex(DARK_SAMUS_ARMOR), LV_PART_MAIN);
	lv_obj_set_style_text_color(button, lv_color_hex(DARK_SAMUS_BLUE), LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_add_event_cb(button, clickme_button_event_cb, LV_EVENT_CLICKED, NULL);

	button_label = lv_label_create(button);
	lv_label_set_text(button_label, "CLICK ME!");
	lv_obj_set_style_text_font(button_label, &lv_font_montserrat_28, LV_PART_MAIN);
	lv_obj_center(button_label);
}

static void morphball_animation_cb(void *obj, int32_t value)
{
    lv_obj_set_x((lv_obj_t *)obj, value);
}

static void create_morphball_anim(lv_obj_t *screen)
{
	static lv_obj_t *morph_ball = NULL;
	static lv_anim_t anim;

	lv_display_t *display = lv_obj_get_display(screen);
	int32_t screen_width = lv_display_get_horizontal_resolution(display);
	int32_t screen_height = lv_display_get_vertical_resolution(display);

	int32_t animation_start_x = 0;
	int32_t animation_end_x = screen_width - ANIMATION_BOX_SIZE;
	int32_t animation_y = screen_height - 150U;

	morph_ball = lv_obj_create(screen);

	lv_obj_set_size(morph_ball, ANIMATION_BOX_SIZE, ANIMATION_BOX_SIZE);
	lv_obj_set_style_bg_color(morph_ball, lv_color_hex(SAMUS_ORANGE), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(morph_ball, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_radius(morph_ball, (ANIMATION_BOX_SIZE / 2U), LV_PART_MAIN);
	lv_obj_set_style_border_width(morph_ball, 0, LV_PART_MAIN);

	lv_obj_set_pos(morph_ball, animation_start_x, animation_y);

	lv_anim_init(&anim);
	lv_anim_set_var(&anim, morph_ball);
	lv_anim_set_exec_cb(&anim, morphball_animation_cb);
	lv_anim_set_values(&anim, animation_start_x, animation_end_x);
	lv_anim_set_duration(&anim, ANIMATION_DURATION_MS);
	lv_anim_set_playback_duration(&anim, ANIMATION_DURATION_MS);
	lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
	lv_anim_start(&anim);
}

static void create_main_screen(lv_obj_t *screen, bool gradient_bg)
{
	if(true == gradient_bg) {
		lv_obj_set_style_bg_color(screen, lv_color_hex(DARK_SAMUS_BLUE), LV_PART_MAIN);
		lv_obj_set_style_bg_grad_color(screen, lv_color_hex(DARK_SAMUS_ARMOR),LV_PART_MAIN);
		lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
	} else {
		lv_obj_set_style_bg_color(screen, lv_color_hex(DARK_SAMUS_ARMOR), LV_PART_MAIN);
    	lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
	}
}

static void create_version(lv_obj_t *screen, bool show_signature)
{
	static lv_obj_t *version_label = NULL;
	static lv_obj_t *label = NULL;
	char version_text[VERSION_TEXT_SIZE] = {0};

	lv_display_t *display = lv_obj_get_display(screen);
	int32_t screen_width = lv_display_get_horizontal_resolution(display);
	int32_t screen_height = lv_display_get_vertical_resolution(display);

	if (screen_height > screen_width) {
		(void)snprintf(version_text, sizeof(version_text),
			       "Zephyr %u.%u\nLVGL %u.%u",
			       (unsigned int)KERNEL_VERSION_MAJOR,
			       (unsigned int)KERNEL_VERSION_MINOR,
			       (unsigned int)lv_version_major(),
			       (unsigned int)lv_version_minor());
	} else {
		(void)snprintf(version_text, sizeof(version_text),
			       "Zephyr %u.%u x LVGL %u.%u",
			       (unsigned int)KERNEL_VERSION_MAJOR,
			       (unsigned int)KERNEL_VERSION_MINOR,
			       (unsigned int)lv_version_major(),
			       (unsigned int)lv_version_minor());
	}

	version_label = lv_label_create(screen);
	lv_label_set_text(version_label, version_text);
	lv_obj_set_style_text_font(version_label, &lv_font_montserrat_48, LV_PART_MAIN);
	lv_obj_set_style_text_color(version_label, lv_color_hex(SAMUS_ORANGE), LV_PART_MAIN);
	lv_obj_set_style_text_align(version_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
	lv_obj_center(version_label);

	if (true == show_signature) {
		label = lv_label_create(screen);
		lv_label_set_text(label, "@_art_of_electronics_");
		lv_obj_set_style_text_font(label, &lv_font_montserrat_28, LV_PART_MAIN);
		lv_obj_set_style_text_color(label, lv_color_hex(SAMUS_RED), LV_PART_MAIN);
		lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -20);
	}
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

    ret = cyd_st7796s_configure();
    if (0 != ret) {
        LOG_ERR("LCD configuration failed: %d", ret);
        return ret;
    }

    ret = display_blanking_off(display);
    if (0 != ret) {
        LOG_ERR("Display blanking off failed: %d", ret);
        return ret;
    }

    screen = lv_screen_active();
	if (NULL == screen) {
    	LOG_ERR("LVGL active screen is NULL");
    	return -ENODEV;
	}

	LOG_INF("LCD RGB started");

    create_main_screen(screen, true);
	create_version(screen, true);
    create_morphball_anim(screen);
    create_clickme(screen);

    return 0;
}

void demo_lcd_loop(void) 
{
    (void)lv_timer_handler();

    k_msleep(LVGL_TASK_DELAY_MS);
}
