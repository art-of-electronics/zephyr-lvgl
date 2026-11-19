/**
 * @file gui.c
 * @brief LVGL widgets, styling, and Morph Ball animation for CYD.
 */

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <zephyr/logging/log.h>
#include <zephyr/version.h>

#include <gui.h>
#include <theme.h>


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
LOG_MODULE_REGISTER(gui, LOG_LEVEL_INF);

#define SHOW_SIGNATURE  (true)
#define GRADIENT_BG     (true)

#define VERSION_TEXT_SIZE       (64U)
#define ANIMATION_DURATION_MS   (3000U)
#define ANIMATION_BOX_SIZE      (60)
#define ANIMATION_BOTTOM_OFFSET (130)
#define COUNTER_POSITION_X      (20)
#define COUNTER_POSITION_Y      (20)
#define BUTTON_WIDTH            (160)
#define BUTTON_HEIGHT           (50)
#define BUTTON_PORTRAIT_Y       (60)
#define BUTTON_LANDSCAPE_Y      (20)
#define BUTTON_MARGIN_RIGHT     (20)
#define BUTTON_RADIUS           (10)
#define SIGNATURE_MARGIN_BOTTOM (20)


/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/
static lv_obj_t *counter_label = NULL;
static uint32_t click_count = 0U;


/*****************************************************************************
                         PRIVATE FUNCTION DECLARATIONS
*****************************************************************************/

/*****************************************************************************
                       PRIVATE FUNCTION IMPLEMENTATIONS
*****************************************************************************/
static void clickme_button_event_cb(lv_event_t *event)
{
    if (LV_EVENT_CLICKED == lv_event_get_code(event)) {
        click_count++;
        lv_label_set_text_fmt(counter_label, "Clicks: %u", (unsigned int)click_count);

        LOG_INF("Button clicked: %u", (unsigned int)click_count);
    }
}

static void create_clickme(lv_obj_t *screen)
{
    lv_obj_t *button = NULL;
    lv_obj_t *button_label = NULL;
    lv_display_t *lv_display = lv_obj_get_display(screen);
    int32_t screen_width = lv_display_get_horizontal_resolution(lv_display);
    int32_t screen_height = lv_display_get_vertical_resolution(lv_display);

    counter_label = lv_label_create(screen);
    lv_label_set_text(counter_label, "Clicks: 0");
    lv_obj_set_style_text_font(counter_label, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(counter_label, lv_color_hex(SAMUS_PURPLE), LV_PART_MAIN);
    lv_obj_align(counter_label, LV_ALIGN_TOP_LEFT, COUNTER_POSITION_X, COUNTER_POSITION_Y);

    button = lv_obj_create(screen);
    lv_obj_set_size(button, BUTTON_WIDTH, BUTTON_HEIGHT);
    if (screen_height > screen_width) {
        lv_obj_align(button, LV_ALIGN_TOP_RIGHT, -BUTTON_MARGIN_RIGHT, BUTTON_PORTRAIT_Y);
    } else {
        lv_obj_align(button, LV_ALIGN_TOP_RIGHT, -BUTTON_MARGIN_RIGHT, BUTTON_LANDSCAPE_Y);
    }

    lv_obj_set_scrollable(button, false);
    lv_obj_set_clickable(button, true);
    lv_obj_set_style_bg_color(button, lv_color_hex(SAMUS_VARIA_ORANGE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(DARK_SAMUS_ARMOR), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(button, BUTTON_RADIUS, LV_PART_MAIN);
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
    lv_obj_t *morph_ball = NULL;
    static lv_anim_t anim = {0};

    lv_display_t *lv_display = lv_obj_get_display(screen);
    int32_t screen_width = lv_display_get_horizontal_resolution(lv_display);
    int32_t screen_height = lv_display_get_vertical_resolution(lv_display);
    int32_t animation_start_x = 0;
    int32_t animation_end_x = screen_width - ANIMATION_BOX_SIZE;
    int32_t animation_y = screen_height - ANIMATION_BOTTOM_OFFSET;

    morph_ball = lv_obj_create(screen);
    lv_obj_set_size(morph_ball, ANIMATION_BOX_SIZE, ANIMATION_BOX_SIZE);
    lv_obj_set_style_bg_color(morph_ball, lv_color_hex(SAMUS_VARIA_ORANGE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(morph_ball, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(morph_ball, (ANIMATION_BOX_SIZE / 2), LV_PART_MAIN);
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
    if (true == gradient_bg) {
        lv_obj_set_style_bg_color(screen, lv_color_hex(DARK_SAMUS_BLUE), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_color(screen, lv_color_hex(DARK_SAMUS_ARMOR), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_color(screen, lv_color_hex(DARK_SAMUS_ARMOR), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    }
}

static void create_version(lv_obj_t *screen, bool show_signature)
{
    lv_obj_t *version_label = NULL;
    lv_obj_t *label = NULL;
    char version_text[VERSION_TEXT_SIZE] = {0};
    lv_display_t *lv_display = lv_obj_get_display(screen);
    int32_t screen_width = lv_display_get_horizontal_resolution(lv_display);
    int32_t screen_height = lv_display_get_vertical_resolution(lv_display);

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
    lv_obj_set_style_text_font(version_label, &lv_font_montserrat_40, LV_PART_MAIN);
    lv_obj_set_style_text_color(version_label, lv_color_hex(SAMUS_VARIA_ORANGE), LV_PART_MAIN);
    lv_obj_set_style_text_align(version_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(version_label);

    if (true == show_signature) {
        label = lv_label_create(screen);
        lv_label_set_text(label, "@_art_of_electronics_");
        lv_obj_set_style_text_font(label, &lv_font_montserrat_28, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_hex(SAMUS_RED), LV_PART_MAIN);
        lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -SIGNATURE_MARGIN_BOTTOM);
    }
}


/*****************************************************************************
                           INTERFACE IMPLEMENTATION
*****************************************************************************/
void gui_init(lv_obj_t *screen)
{
    click_count = 0U;
    create_main_screen(screen, GRADIENT_BG);
    create_version(screen, SHOW_SIGNATURE);
    create_morphball_anim(screen);
    create_clickme(screen);
}
