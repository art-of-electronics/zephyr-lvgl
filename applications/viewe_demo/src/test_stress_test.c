#include <stdint.h>
#include <lvgl.h>

#define STRESS_LEFT_X		40
#define STRESS_TOP_Y		40
#define STRESS_BOTTOM_Y		180
#define STRESS_RIGHT_X		680
#define STRESS_COUNTER_OFFSET_X	(-20)
#define STRESS_COUNTER_OFFSET_Y	(-20)
#define STRESS_COLOR_START	0
#define STRESS_COLOR_END	255
#define STRESS_COLOR_RED	0xFF0000U
#define STRESS_COLOR_GREEN	0x00FF00U
#define STRESS_COLOR_TEXT   0xF28C28U

#define STRESS_BOX_SIZE         60
#define STRESS_ANIM_DURATION_MS 650
#define STRESS_COUNTER_PERIOD   100

static void stress_animation_y_cb(void *obj, int32_t value)
{
	lv_obj_set_y((lv_obj_t *)obj, value);
}

static void stress_animation_color_cb(void *obj, int32_t value)
{
	lv_obj_set_style_bg_color((lv_obj_t *)obj, lv_color_mix(lv_color_hex(STRESS_COLOR_RED),
					          lv_color_hex(STRESS_COLOR_GREEN), (uint8_t)value), LV_PART_MAIN);
}

static void stress_counter_cb(lv_timer_t *timer)
{
	lv_obj_t *label = (lv_obj_t *)lv_timer_get_user_data(timer);
	static uint32_t counter = 0U;

	counter++;
	lv_label_set_text_fmt(label, "Frames: %u", (unsigned int)counter);
}

void stress_test_start(lv_obj_t *screen)
{
	lv_obj_t *stress_box = NULL;
	lv_obj_t *color_box = NULL;
	lv_obj_t *counter_label = NULL;
	lv_anim_t stress_anim;
	lv_anim_t color_anim;

	if (NULL == screen) {
		return;
	}

	/* Vertical animation — left side of the screen. */
	stress_box = lv_obj_create(screen);
	lv_obj_set_size(stress_box, STRESS_BOX_SIZE, STRESS_BOX_SIZE);
	lv_obj_set_pos(stress_box, STRESS_LEFT_X, STRESS_TOP_Y);
	lv_obj_set_style_bg_color(stress_box, lv_color_hex(STRESS_COLOR_GREEN), LV_PART_MAIN);
	lv_obj_set_style_border_width(stress_box, 0, LV_PART_MAIN);
	lv_obj_set_style_radius(stress_box, 0, LV_PART_MAIN);

	lv_anim_init(&stress_anim);
	lv_anim_set_var(&stress_anim, stress_box);
	lv_anim_set_exec_cb(&stress_anim, stress_animation_y_cb);
	lv_anim_set_values(&stress_anim, STRESS_TOP_Y, STRESS_BOTTOM_Y);
	lv_anim_set_duration(&stress_anim, STRESS_ANIM_DURATION_MS);
	lv_anim_set_playback_duration(&stress_anim, STRESS_ANIM_DURATION_MS);
	lv_anim_set_repeat_count(&stress_anim, LV_ANIM_REPEAT_INFINITE);
	lv_anim_start(&stress_anim);

	/* Color animation — right side of the screen. */
	color_box = lv_obj_create(screen);
	lv_obj_set_size(color_box, STRESS_BOX_SIZE, STRESS_BOX_SIZE);
	lv_obj_set_pos(color_box, STRESS_RIGHT_X, STRESS_TOP_Y);
	lv_obj_set_style_border_width(color_box, 0, LV_PART_MAIN);
	lv_obj_set_style_radius(color_box, 0, LV_PART_MAIN);

	lv_anim_init(&color_anim);
	lv_anim_set_var(&color_anim, color_box);
	lv_anim_set_exec_cb(&color_anim, stress_animation_color_cb);
	lv_anim_set_values(&color_anim, STRESS_COLOR_START, STRESS_COLOR_END);
	lv_anim_set_duration(&color_anim, STRESS_ANIM_DURATION_MS);
	lv_anim_set_playback_duration(&color_anim, STRESS_ANIM_DURATION_MS);
	lv_anim_set_repeat_count(&color_anim, LV_ANIM_REPEAT_INFINITE);
	lv_anim_start(&color_anim);

	/* Independent text updates. */
	counter_label = lv_label_create(screen);
	lv_label_set_text(counter_label, "Frames: 0");
	lv_obj_set_style_text_color(counter_label, lv_color_hex(STRESS_COLOR_TEXT), LV_PART_MAIN);
	lv_obj_align(counter_label, LV_ALIGN_BOTTOM_RIGHT, STRESS_COUNTER_OFFSET_X, STRESS_COUNTER_OFFSET_Y);

	(void)lv_timer_create(stress_counter_cb, STRESS_COUNTER_PERIOD, counter_label);
}
