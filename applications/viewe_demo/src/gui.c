/**
 * @file gui.c
 * @brief Shared LVGL widgets and responsive layout for VIEWE RGB LCDs.
 */

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include <zephyr/logging/log.h>
#include <zephyr/version.h>

#include <gui.h>
#include <theme.h>


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
LOG_MODULE_REGISTER(viewe_gui, LOG_LEVEL_INF);

#define GUI_COMPACT_MAX_SHORT_EDGE  (399)
#define GUI_VERSION_TEXT_SIZE       (64U)
#define GUI_ANIMATION_DURATION_MS   (3000U)
#define GUI_BUTTON_RIGHT_OFFSET     (-20)
#define GUI_COUNTER_LEFT_OFFSET     (20)
#define GUI_COUNTER_TOP_OFFSET      (20)
#define GUI_BUTTON_RADIUS           (10)
#define GUI_BUTTON_TEXT             "CLICK ME!"
#define GUI_SIGNATURE_TEXT          "@_art_of_electronics_"

/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/
struct gui_layout {
    int32_t morph_ball_size;
    int32_t morph_ball_bottom_offset;
    int32_t button_width;
    int32_t button_height;
    int32_t button_top_offset;
    int32_t button_portrait_top_offset;
    int32_t signature_bottom_offset;
    const lv_font_t *counter_font;
    const lv_font_t *version_font;
    const lv_font_t *signature_font;
};

static const struct gui_layout layout_large = {
    .morph_ball_size = 100,
    .morph_ball_bottom_offset = 200,
    .button_width = 180,
    .button_height = 60,
    .button_top_offset = 20,
    .button_portrait_top_offset = 20,
    .signature_bottom_offset = 50,
    .counter_font = &lv_font_montserrat_48,
    .version_font = &lv_font_montserrat_48,
    .signature_font = &lv_font_montserrat_28,
};

static const struct gui_layout layout_compact = {
    .morph_ball_size = 40,
    .morph_ball_bottom_offset = 100,
    .button_width = 160,
    .button_height = 50,
    .button_top_offset = 20,
    .button_portrait_top_offset = 60,
    .signature_bottom_offset = 20,
    .counter_font = &lv_font_montserrat_28,
    .version_font = &lv_font_montserrat_28,
    .signature_font = &lv_font_montserrat_20,
};

static lv_obj_t *counter_label = NULL;
static uint32_t click_count = 0U;


/*****************************************************************************
                         PRIVATE FUNCTION DECLARATIONS
*****************************************************************************/

/*****************************************************************************
                       PRIVATE FUNCTION IMPLEMENTATIONS
*****************************************************************************/
static const struct gui_layout *gui_select_layout(int32_t width, int32_t height)
{
    int32_t short_edge = (width < height) ? (width) : (height);

    return (short_edge <= GUI_COMPACT_MAX_SHORT_EDGE) ? (&layout_compact) : (&layout_large);
}

static void gui_clickme_event_cb(lv_event_t *event)
{
    if (LV_EVENT_CLICKED == lv_event_get_code(event)) {
        click_count++;
        lv_label_set_text_fmt(counter_label, "Clicks: %u", (unsigned int)click_count);
        LOG_INF("Button clicked: %u", (unsigned int)click_count);
    }
}

static void gui_create_clickme(lv_obj_t *screen, const struct gui_layout *layout, bool portrait)
{
    lv_obj_t *button = NULL;
    lv_obj_t *button_label = NULL;
    int32_t top_offset = layout->button_top_offset;

    counter_label = lv_label_create(screen);
    lv_label_set_text(counter_label, "Clicks: 0");
    lv_obj_set_style_text_font(counter_label, layout->counter_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(counter_label, lv_color_hex(SAMUS_PURPLE), LV_PART_MAIN);
    lv_obj_align(counter_label, LV_ALIGN_TOP_LEFT, GUI_COUNTER_LEFT_OFFSET, GUI_COUNTER_TOP_OFFSET);

    button = lv_obj_create(screen);
    lv_obj_set_size(button, layout->button_width, layout->button_height);
    if (true == portrait) {
        top_offset = layout->button_portrait_top_offset;
    }
    lv_obj_align(button, LV_ALIGN_TOP_RIGHT, GUI_BUTTON_RIGHT_OFFSET, top_offset);
    lv_obj_set_scrollable(button, false);
    lv_obj_set_clickable(button, true);
    lv_obj_set_style_bg_color(button, lv_color_hex(SAMUS_VARIA_ORANGE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(DARK_SAMUS_ARMOR), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(button, GUI_BUTTON_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_text_color(button, lv_color_hex(DARK_SAMUS_ARMOR), LV_PART_MAIN);
    lv_obj_set_style_text_color(button, lv_color_hex(DARK_SAMUS_BLUE), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(button, gui_clickme_event_cb, LV_EVENT_CLICKED, NULL);

    button_label = lv_label_create(button);
    lv_label_set_text(button_label, GUI_BUTTON_TEXT);
    lv_obj_set_style_text_font(button_label, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_center(button_label);
}

static void gui_morphball_animation_cb(void *object, int32_t value)
{
    lv_obj_set_x((lv_obj_t *)object, value);
}

static void gui_create_morphball_anim(lv_obj_t *screen, const struct gui_layout *layout,
                                      int32_t screen_width, int32_t screen_height)
{
    lv_obj_t *morph_ball = NULL;
    lv_anim_t anim = {0};
    int32_t animation_start_x = 0;
    int32_t animation_end_x = screen_width - layout->morph_ball_size;
    int32_t animation_y = screen_height - layout->morph_ball_bottom_offset;

    morph_ball = lv_obj_create(screen);
    lv_obj_set_size(morph_ball, layout->morph_ball_size, layout->morph_ball_size);
    lv_obj_set_style_bg_color(morph_ball, lv_color_hex(SAMUS_VARIA_ORANGE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(morph_ball, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(morph_ball, layout->morph_ball_size / 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(morph_ball, 0, LV_PART_MAIN);
    lv_obj_set_pos(morph_ball, animation_start_x, animation_y);

    lv_anim_init(&anim);
    lv_anim_set_var(&anim, morph_ball);
    lv_anim_set_exec_cb(&anim, gui_morphball_animation_cb);
    lv_anim_set_values(&anim, animation_start_x, animation_end_x);
    lv_anim_set_duration(&anim, GUI_ANIMATION_DURATION_MS);
    lv_anim_set_playback_duration(&anim, GUI_ANIMATION_DURATION_MS);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&anim);
}

static void gui_create_version(lv_obj_t *screen, const struct gui_layout *layout, bool portrait)
{
    lv_obj_t *version_label = NULL;
    lv_obj_t *signature_label = NULL;
    char version_text[GUI_VERSION_TEXT_SIZE] = {0};

    if (true == portrait) {
        (void)snprintf(version_text,
                       sizeof(version_text),
                       "Zephyr %u.%u\nLVGL %u.%u",
                       (unsigned int)KERNEL_VERSION_MAJOR,
                       (unsigned int)KERNEL_VERSION_MINOR,
                       (unsigned int)lv_version_major(),
                       (unsigned int)lv_version_minor());
    } else {
        (void)snprintf(version_text,
                       sizeof(version_text),
                       "Zephyr %u.%u x LVGL %u.%u",
                       (unsigned int)KERNEL_VERSION_MAJOR,
                       (unsigned int)KERNEL_VERSION_MINOR,
                       (unsigned int)lv_version_major(),
                       (unsigned int)lv_version_minor());
    }

    version_label = lv_label_create(screen);
    lv_label_set_text(version_label, version_text);
    lv_obj_set_style_text_font(version_label, layout->version_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(version_label, lv_color_hex(SAMUS_VARIA_ORANGE), LV_PART_MAIN);
    lv_obj_set_style_text_align(version_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(version_label);

    signature_label = lv_label_create(screen);
    lv_label_set_text(signature_label, GUI_SIGNATURE_TEXT);
    lv_obj_set_style_text_font(signature_label, layout->signature_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(signature_label, lv_color_hex(SAMUS_RED), LV_PART_MAIN);
    lv_obj_align(signature_label, LV_ALIGN_BOTTOM_MID, 0, -layout->signature_bottom_offset);
}

static void gui_create_background(lv_obj_t *screen)
{
    lv_obj_set_style_bg_color(screen, lv_color_hex(DARK_SAMUS_BLUE), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(DARK_SAMUS_ARMOR), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
}


/*****************************************************************************
                           INTERFACE IMPLEMENTATION
*****************************************************************************/
int gui_init(lv_obj_t *screen)
{
    lv_display_t *display = NULL;
    const struct gui_layout *layout = NULL;
    int32_t screen_width = 0;
    int32_t screen_height = 0;
    bool portrait = false;

    if (NULL == screen) {
        return -EINVAL;
    }

    display = lv_obj_get_display(screen);
    if (NULL == display) {
        return -ENODEV;
    }

    screen_width = lv_display_get_horizontal_resolution(display);
    screen_height = lv_display_get_vertical_resolution(display);
    layout = gui_select_layout(screen_width, screen_height);
    portrait = (screen_height > screen_width);

    gui_create_background(screen);
    gui_create_version(screen, layout, portrait);
    gui_create_morphball_anim(screen, layout, screen_width, screen_height);
    gui_create_clickme(screen, layout, portrait);

    LOG_INF("GUI initialized: %ld x %ld (%s)",
            (long)screen_width, (long)screen_height,
            (layout == &layout_compact) ? ("compact") : ("large"));

    return 0;
}
