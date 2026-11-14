/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <lvgl.h>

#include <gui_wifi.h>
#include <gui_wifi_scan.h>
#include <gui_wifi_register.h>
#include <wifi_connection.h>
#include <gui_wifi_register.h>
#include <backlight.h>


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
#define WIFI_BUTTON_WIDTH       (130)
#define WIFI_BUTTON_HEIGHT      (45)
#define WIFI_BUTTON_OFFSET_X    (80)
#define WIFI_BUTTON_OFFSET_Y    (-15)

#define WIFI_CLOSE_BUTTON_SIZE  (36)
#define WIFI_CLOSE_OFFSET       (10)

#define WIFI_TITLE_OFFSET_Y     (15)

#define WIFI_INFO_MAC_X         (15)
#define WIFI_INFO_MAC_Y         (70)
#define WIFI_INFO_IP_X          (15)
#define WIFI_INFO_IP_Y          (125)

#define WIFI_INFO_TEXT_SIZE     (48U)

#define WIFI_SCREEN_FADE_MS     (500U)
#define WIFI_INFO_REFRESH_PERIOD_MS (1000U)


/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/
static lv_obj_t *scan_screen = NULL;
static lv_obj_t *register_screen = NULL;

static lv_obj_t *fade_overlay = NULL;
static lv_obj_t *fade_target = NULL;
static lv_obj_t *fade_source = NULL;
static bool fade_screen_loaded = false;

static lv_obj_t *wifi_mac_label = NULL;
static lv_obj_t *wifi_ip_label = NULL;

static lv_obj_t *register_button_label = NULL;

static uint32_t fade_start_brightness = 0U;


/*****************************************************************************
                         PRIVATE FUNCTION DECLARATIONS
*****************************************************************************/

/*****************************************************************************
                       PRIVATE FUNCTION IMPLEMENTATIONS
*****************************************************************************/
static void gui_wifi_fade_exec(void *obj, int32_t brightness)
{
    (void)obj;
    (void)backlight_set_brightness((uint32_t)brightness);

    if ((true != fade_screen_loaded) &&
        (0 == brightness) && (NULL != fade_target)) {
        fade_screen_loaded = true;
        lv_screen_load(fade_target);
    }
}

static void gui_wifi_fade_completed(lv_anim_t *anim)
{
    (void)anim;

    (void)backlight_set_brightness(fade_start_brightness);

    if (NULL != fade_overlay) {
        lv_obj_delete(fade_overlay);
        fade_overlay = NULL;
    }

    fade_target = NULL;
    fade_screen_loaded = false;

    if ((scan_screen == fade_source) && (scan_screen != lv_screen_active())) {
        gui_wifi_scan_close();
    }

    if ((register_screen == fade_source) && (register_screen != lv_screen_active())) {
        gui_wifi_register_close();
    }

    fade_source = NULL;

    if (scan_screen == lv_screen_active()) {
        gui_wifi_scan_start();
    }

    if (register_screen == lv_screen_active()) {
        gui_wifi_register_open();
    }
}

static void gui_wifi_switch_screen(lv_obj_t *target)
{
    lv_anim_t anim;

    if ((NULL == target) || (NULL != fade_overlay) ||
        (target == lv_screen_active())) {
        return;
    }

    fade_start_brightness = backlight_get_brightness();

    fade_overlay = lv_obj_create(lv_layer_top());
    if (NULL == fade_overlay) {
        lv_screen_load(target);
        return;
    }

    fade_target = target;
    fade_screen_loaded = false;
    fade_source = lv_screen_active();

    lv_obj_remove_style_all(fade_overlay);
    lv_obj_set_size(fade_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(fade_overlay, 0, 0);

    lv_obj_set_scrollable(fade_overlay, false);
    lv_obj_set_clickable(fade_overlay, true);

    lv_obj_set_style_bg_opa(fade_overlay, LV_OPA_TRANSP, LV_PART_MAIN);
    
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, fade_overlay);
    lv_anim_set_exec_cb(&anim, gui_wifi_fade_exec);
    lv_anim_set_values(&anim, (int32_t)fade_start_brightness, 0);
    lv_anim_set_duration(&anim, WIFI_SCREEN_FADE_MS);
    lv_anim_set_playback_duration(&anim, WIFI_SCREEN_FADE_MS);

    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
    lv_anim_set_completed_cb(&anim, gui_wifi_fade_completed);

    if (NULL == lv_anim_start(&anim)) {
        lv_obj_delete(fade_overlay);

        fade_overlay = NULL;
        fade_target = NULL;
        fade_screen_loaded = false;

        lv_screen_load(target);
        (void)backlight_set_brightness(fade_start_brightness);
    }
}

static void gui_wifi_navigate(lv_event_t *event)
{
    lv_obj_t *target = (lv_obj_t *)lv_event_get_user_data(event);
    gui_wifi_switch_screen(target);
}

static lv_obj_t *gui_wifi_create_button(lv_obj_t *parent, const char *text, lv_obj_t *target,
                                        lv_color_t background, lv_color_t foreground)
{
    lv_obj_t *button = lv_button_create(parent);
    if (NULL == button) {
        return NULL;
    }

    lv_obj_t *label = lv_label_create(button);
    if (NULL == label) {
        lv_obj_delete(button);
        return NULL;
    }

    lv_obj_set_style_bg_color(button, background, LV_PART_MAIN);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, foreground, LV_PART_MAIN);
    lv_obj_center(label);

    lv_obj_add_event_cb(button, gui_wifi_navigate, LV_EVENT_CLICKED, target);

    return button;
}

static lv_obj_t *gui_wifi_create_screen(lv_obj_t *home, const char *title,
                                        lv_color_t background, lv_color_t accent)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    if (NULL == screen) {
        return NULL;
    }

    lv_obj_set_style_bg_color(screen, background, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollable(screen, false);

    lv_obj_t *label = lv_label_create(screen);
    if (NULL == label) {
        lv_obj_delete(screen);
        return NULL;
    }

    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, accent, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, WIFI_TITLE_OFFSET_Y);

    lv_obj_t *close = gui_wifi_create_button(screen, "X", home, accent, background);
    if (NULL == close) {
        lv_obj_delete(screen);
        return NULL;
    }

    lv_obj_set_size(close, WIFI_CLOSE_BUTTON_SIZE, WIFI_CLOSE_BUTTON_SIZE);
    lv_obj_align(close, LV_ALIGN_TOP_RIGHT, -WIFI_CLOSE_OFFSET, WIFI_CLOSE_OFFSET);

    return screen;
}

static int wifi_gui_create_info_labels(lv_obj_t *screen, lv_color_t accent)
{
    if (NULL == screen) {
        return -EINVAL;
    }

    wifi_mac_label = lv_label_create(screen);
    if (NULL == wifi_mac_label) {
        return -ENOMEM;
    }

    lv_obj_set_pos(wifi_mac_label, WIFI_INFO_MAC_X, WIFI_INFO_MAC_Y);

    lv_obj_set_style_text_color(wifi_mac_label, accent, LV_PART_MAIN);
    lv_obj_set_style_text_font(wifi_mac_label, &lv_font_montserrat_36, LV_PART_MAIN);
    lv_label_set_text(wifi_mac_label, "MAC: --");

    wifi_ip_label = lv_label_create(screen);
    if (NULL == wifi_ip_label) {
        return -ENOMEM;
    }

    lv_obj_set_pos(wifi_ip_label, WIFI_INFO_IP_X, WIFI_INFO_IP_Y);

    lv_obj_set_style_text_color(wifi_ip_label, accent, LV_PART_MAIN);
    lv_obj_set_style_text_font(wifi_ip_label, &lv_font_montserrat_36, LV_PART_MAIN);
    lv_label_set_text(wifi_ip_label, "IP: --");

    return 0;
}

static void wifi_gui_update_info(void)
{
    char mac[WIFI_CONNECTION_MAC_TEXT_SIZE] = {0};
    char ip[WIFI_CONNECTION_IPV4_TEXT_SIZE] = {0};
    char text[WIFI_INFO_TEXT_SIZE] = {0};

    if (NULL != wifi_mac_label) {
        if (0 == wifi_connection_get_mac(mac, sizeof(mac))) {
            (void)snprintf(text, sizeof(text), "MAC: %s", mac);
        }
        else {
            (void)snprintf(text, sizeof(text), "MAC: --");
        }

        lv_label_set_text(wifi_mac_label, text);
    }

    if (NULL != wifi_ip_label) {
        if (0 == wifi_connection_get_ipv4(ip, sizeof(ip))) {
            (void)snprintf(text, sizeof(text), "IP: %s", ip);
        }
        else {
            (void)snprintf(text, sizeof(text), "IP: --");
        }

        lv_label_set_text(wifi_ip_label, text);
    }
}

static void wifi_gui_update_register_button(void)
{
    enum wifi_connection_state state = wifi_connection_get_state();
    const char *text = "REGISTER";

    if (NULL == register_button_label) {
        return;
    }

    if ((WIFI_CONNECTION_CONNECTED == state) ||
        (WIFI_CONNECTION_DISCONNECTING == state)) {
        text = "WI-FI";
    }

    lv_label_set_text(register_button_label, text);
    lv_obj_center(register_button_label);
}


/*****************************************************************************
                           INTERFACE IMPLEMENTATION
*****************************************************************************/
int gui_wifi_init(lv_obj_t *screen, lv_color_t background, lv_color_t accent)
{
    lv_obj_t *scan_button = NULL;
    lv_obj_t *register_button = NULL;
    int ret = 0;

    if (NULL == screen) {
        return -EINVAL;
    }

    if ((NULL != scan_screen) || (NULL != register_screen)) {
        return -EALREADY;
    }

    ret = wifi_connection_init();
    if (0 != ret) {
        return ret;
    }

    scan_screen = gui_wifi_create_screen(screen, "Wi-Fi Scan", background, accent);
    if (NULL == scan_screen) {
        return -ENOMEM;
    }

    register_screen = gui_wifi_create_screen(screen, "Wi-Fi Register", background, accent);
    if (NULL == register_screen) {
        lv_obj_delete(scan_screen);
        scan_screen = NULL;

        return -ENOMEM;
    }

    ret = gui_wifi_scan_init(scan_screen, accent);
    if (0 != ret) {
        lv_obj_delete(scan_screen);
        lv_obj_delete(register_screen);

        scan_screen = NULL;
        register_screen = NULL;

        return ret;
    }

    ret = gui_wifi_register_init(register_screen, accent);
    if (0 != ret) {
        lv_obj_delete(scan_screen);
        lv_obj_delete(register_screen);

        scan_screen = NULL;
        register_screen = NULL;

        return ret;
    }

    scan_button = gui_wifi_create_button(screen, "SCAN", scan_screen, accent, background);
    register_button = gui_wifi_create_button(screen, "REGISTER", register_screen, accent, background);

    if ((NULL == scan_button) || (NULL == register_button)) {
        if (NULL != scan_button) {
            lv_obj_delete(scan_button);
        }

        if (NULL != register_button) {
            lv_obj_delete(register_button);
        }

        lv_obj_delete(scan_screen);
        lv_obj_delete(register_screen);

        scan_screen = NULL;
        register_screen = NULL;

        return -ENOMEM;
    }

    register_button_label = lv_obj_get_child(register_button, 0);

    ret = wifi_gui_create_info_labels(screen, accent);
    if (0 != ret) {
        return ret;
    }

    wifi_gui_update_info();
    wifi_gui_update_register_button();

    lv_obj_set_size(scan_button, WIFI_BUTTON_WIDTH, WIFI_BUTTON_HEIGHT);
    lv_obj_align(scan_button, LV_ALIGN_BOTTOM_MID, -WIFI_BUTTON_OFFSET_X, WIFI_BUTTON_OFFSET_Y);

    lv_obj_set_size(register_button, WIFI_BUTTON_WIDTH, WIFI_BUTTON_HEIGHT);
    lv_obj_align(register_button, LV_ALIGN_BOTTOM_MID, WIFI_BUTTON_OFFSET_X, WIFI_BUTTON_OFFSET_Y);

    return 0;
}

void gui_wifi_process(void)
{
    static enum wifi_connection_state previous_state = WIFI_CONNECTION_DISCONNECTED;
    static uint32_t last_update = 0U;

    enum wifi_connection_state state = WIFI_CONNECTION_DISCONNECTED;
    bool state_changed = false;

    gui_wifi_register_process();

    state = wifi_connection_get_state();
    state_changed = (previous_state != state);

    if ((true == state_changed) ||
        (WIFI_INFO_REFRESH_PERIOD_MS <= lv_tick_elaps(last_update))) {

        wifi_gui_update_info();
        wifi_gui_update_register_button();

        previous_state = state;
        last_update = lv_tick_get();
    }
}
