/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <errno.h>
#include <stdint.h>

#include <lvgl.h>
#include <theme.h>
#include <gui_wifi_register.h>
#include <wifi_connection.h>


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
#define WIFI_REGISTER_SSID_X                 (15)
#define WIFI_REGISTER_PASSWORD_X             (210)
#define WIFI_REGISTER_CONNECT_X              (375)
#define WIFI_REGISTER_FIELDS_Y               (55)
#define WIFI_REGISTER_SSID_WIDTH             (185)
#define WIFI_REGISTER_PASSWORD_WIDTH         (155)
#define WIFI_REGISTER_CONNECT_WIDTH          (90)
#define WIFI_REGISTER_FIELDS_HEIGHT          (34)

#define WIFI_REGISTER_STATUS_X               (15)
#define WIFI_REGISTER_STATUS_Y               (96)
#define WIFI_REGISTER_STATUS_WIDTH           (450)

#define WIFI_REGISTER_KEYBOARD_X             (10)
#define WIFI_REGISTER_KEYBOARD_Y             (115)
#define WIFI_REGISTER_KEYBOARD_WIDTH         (460)
#define WIFI_REGISTER_KEYBOARD_HEIGHT        (147)

#define WIFI_REGISTER_SSID_MAX_LENGTH        (32U)
#define WIFI_REGISTER_PASSPHRASE_MAX_LENGTH  (63U)
#define WIFI_REGISTER_FIELD_PAD_H            (7)
#define WIFI_REGISTER_FIELD_PAD_V            (6)
#define WIFI_REGISTER_KEYBOARD_PAD           (2)
#define WIFI_REGISTER_KEYBOARD_GAP           (2)
#define WIFI_REGISTER_KEYBOARD_RADIUS        (3)
#define WIFI_REGISTER_FIELD_RADIUS           (4)


/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/
static lv_obj_t *register_screen = NULL;
static lv_obj_t *ssid_textarea = NULL;
static lv_obj_t *password_textarea = NULL;
static lv_obj_t *register_keyboard = NULL;
static lv_obj_t *register_status = NULL;


/*****************************************************************************
                         PRIVATE FUNCTION DECLARATIONS
*****************************************************************************/

/*****************************************************************************
                       PRIVATE FUNCTION IMPLEMENTATIONS
*****************************************************************************/
static void gui_wifi_register_set_status(const char *text)
{
    if (NULL != register_status) {
        lv_label_set_text(register_status, text);
    }
}

static void gui_wifi_register_textarea_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *textarea = (lv_obj_t *)lv_event_get_target(event);

    if (((LV_EVENT_CLICKED == code) || (LV_EVENT_FOCUSED == code)) &&
        (NULL != register_keyboard)) {
        lv_keyboard_set_textarea(register_keyboard, textarea);
    }
}

static void gui_wifi_register_keyboard_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);

    if (LV_EVENT_READY == code) {
        // The keyboard's OK key changes focus, it does not connect.
        if (ssid_textarea == lv_keyboard_get_textarea(register_keyboard)) {
            lv_keyboard_set_textarea(register_keyboard, password_textarea);
            lv_obj_add_state(password_textarea, LV_STATE_FOCUSED);
        }
    }
    else if (LV_EVENT_CANCEL == code) {
        // Keep the on-screen keyboard visible and usable.
        lv_keyboard_set_textarea(register_keyboard, ssid_textarea);
    }
}

static void gui_wifi_register_connect_event(lv_event_t *event)
{
    const char *ssid = NULL;
    const char *password = NULL;
    int ret = 0;

    if (LV_EVENT_CLICKED != lv_event_get_code(event)) {
        return;
    }

    ssid = lv_textarea_get_text(ssid_textarea);
    password = lv_textarea_get_text(password_textarea);

    if ((NULL == ssid) || ('\0' == ssid[0])) {
        gui_wifi_register_set_status("Enter SSID");
        lv_keyboard_set_textarea(register_keyboard, ssid_textarea);
        return;
    }

    if ((NULL == password) || ('\0' == password[0])) {
        gui_wifi_register_set_status("Enter password");
        lv_keyboard_set_textarea(register_keyboard, password_textarea);
        return;
    }

    ret = wifi_connection_connect(ssid, password);

    if (0 != ret) {
        gui_wifi_register_set_status("Connection request failed");
        return;
    }

    gui_wifi_register_set_status("Connecting...");
}

static lv_obj_t *gui_wifi_register_create_textarea(lv_obj_t *screen,
                                                   const char *placeholder,
                                                   int32_t x,
                                                   int32_t width,
                                                   uint32_t max_length,
                                                   bool password_mode,
                                                   lv_color_t accent)
{
    lv_obj_t *textarea = lv_textarea_create(screen);

    if (NULL == textarea) {
        return NULL;
    }

    lv_obj_set_pos(textarea, x, WIFI_REGISTER_FIELDS_Y);
    lv_obj_set_size(textarea, width, WIFI_REGISTER_FIELDS_HEIGHT);
    lv_obj_set_style_bg_color(textarea, lv_color_hex(CHOZO_DARK_BLUE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(textarea, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(textarea, accent, LV_PART_MAIN);
    lv_obj_set_style_border_width(textarea, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(textarea, WIFI_REGISTER_FIELD_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(textarea, WIFI_REGISTER_FIELD_PAD_H, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(textarea, WIFI_REGISTER_FIELD_PAD_V, LV_PART_MAIN);
    lv_obj_set_style_text_font(textarea, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(textarea, lv_color_hex(SAMUS_BLUE), LV_PART_MAIN);
    lv_obj_set_style_text_color(textarea, accent, LV_PART_TEXTAREA_PLACEHOLDER);
    lv_textarea_set_one_line(textarea, true);
    lv_textarea_set_placeholder_text(textarea, placeholder);
    lv_textarea_set_max_length(textarea, max_length);
    lv_textarea_set_password_mode(textarea, password_mode);
    lv_obj_add_event_cb(textarea, gui_wifi_register_textarea_event, 
                        LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(textarea, gui_wifi_register_textarea_event, 
                        LV_EVENT_FOCUSED, NULL);

    return textarea;
}


/*****************************************************************************
                           INTERFACE IMPLEMENTATION
*****************************************************************************/
int gui_wifi_register_init(lv_obj_t *screen, lv_color_t accent)
{
    lv_obj_t *connect_button = NULL;
    lv_obj_t *connect_label = NULL;

    if (NULL == screen) {
        return -EINVAL;
    }
    if (NULL != register_screen) {
        return -EALREADY;
    }

    register_screen = screen;

    ssid_textarea = gui_wifi_register_create_textarea(screen, "SSID",
                                                      WIFI_REGISTER_SSID_X,
                                                      WIFI_REGISTER_SSID_WIDTH,
                                                      WIFI_REGISTER_SSID_MAX_LENGTH,
                                                      false, accent);
    password_textarea = gui_wifi_register_create_textarea(screen, "Password",
                                                          WIFI_REGISTER_PASSWORD_X,
                                                          WIFI_REGISTER_PASSWORD_WIDTH,
                                                          WIFI_REGISTER_PASSPHRASE_MAX_LENGTH,
                                                          true, accent);
    if ((NULL == ssid_textarea) || (NULL == password_textarea)) {
        return -ENOMEM;
    }

    connect_button = lv_button_create(screen);
    if (NULL == connect_button) {
        return -ENOMEM;
    }

    lv_obj_set_pos(connect_button, WIFI_REGISTER_CONNECT_X, WIFI_REGISTER_FIELDS_Y);
    lv_obj_set_size(connect_button, WIFI_REGISTER_CONNECT_WIDTH, 
                    WIFI_REGISTER_FIELDS_HEIGHT);
    lv_obj_set_style_bg_color(connect_button, accent, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(connect_button, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(connect_button, WIFI_REGISTER_FIELD_RADIUS, LV_PART_MAIN);
    lv_obj_add_event_cb(connect_button, gui_wifi_register_connect_event, 
                        LV_EVENT_CLICKED, NULL);

    connect_label = lv_label_create(connect_button);
    if (NULL == connect_label) {
        return -ENOMEM;
    }
    lv_label_set_text(connect_label, "Connect");
    lv_obj_set_style_text_color(connect_label, lv_color_hex(CHOZO_DARK_BLUE), LV_PART_MAIN);
    lv_obj_set_style_text_font(connect_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(connect_label);

    register_status = lv_label_create(screen);
    if (NULL == register_status) {
        return -ENOMEM;
    }
    lv_obj_set_pos(register_status, WIFI_REGISTER_STATUS_X, WIFI_REGISTER_STATUS_Y);
    lv_obj_set_width(register_status, WIFI_REGISTER_STATUS_WIDTH);
    lv_label_set_long_mode(register_status, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(register_status, accent, LV_PART_MAIN);
    lv_obj_set_style_text_font(register_status, &lv_font_montserrat_14, LV_PART_MAIN);
    gui_wifi_register_set_status("Disconnected");

    register_keyboard = lv_keyboard_create(screen);
    if (NULL == register_keyboard) {
        return -ENOMEM;
    }

    lv_obj_set_size(register_keyboard, WIFI_REGISTER_KEYBOARD_WIDTH,
                    WIFI_REGISTER_KEYBOARD_HEIGHT);

    lv_obj_align(register_keyboard, LV_ALIGN_TOP_LEFT,
                 WIFI_REGISTER_KEYBOARD_X, WIFI_REGISTER_KEYBOARD_Y);
    lv_obj_set_size(register_keyboard, WIFI_REGISTER_KEYBOARD_WIDTH,
                    WIFI_REGISTER_KEYBOARD_HEIGHT);
    lv_obj_set_style_bg_color(register_keyboard, lv_color_hex(CHOZO_DARK_BLUE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(register_keyboard, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(register_keyboard, accent, LV_PART_MAIN);
    lv_obj_set_style_border_width(register_keyboard, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(register_keyboard, WIFI_REGISTER_FIELD_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_pad_all(register_keyboard, WIFI_REGISTER_KEYBOARD_PAD, LV_PART_MAIN);
    lv_obj_set_style_pad_row(register_keyboard, WIFI_REGISTER_KEYBOARD_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_column(register_keyboard, WIFI_REGISTER_KEYBOARD_GAP, LV_PART_MAIN);

    lv_obj_set_style_bg_color(register_keyboard, accent, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(register_keyboard, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_radius(register_keyboard, WIFI_REGISTER_KEYBOARD_RADIUS, LV_PART_ITEMS);
    lv_obj_set_style_text_color(register_keyboard, lv_color_hex(CHOZO_DARK_BLUE), LV_PART_ITEMS);
    lv_obj_set_style_text_font(register_keyboard, &lv_font_montserrat_14, LV_PART_ITEMS);

    lv_keyboard_set_textarea(register_keyboard, ssid_textarea);
    lv_obj_add_event_cb(register_keyboard, gui_wifi_register_keyboard_event, 
                        LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(register_keyboard, gui_wifi_register_keyboard_event, 
                        LV_EVENT_CANCEL, NULL);

    return 0;
}

void gui_wifi_register_close(void)
{
    enum wifi_connection_state state = WIFI_CONNECTION_DISCONNECTED;

    if (NULL != ssid_textarea) {
        lv_textarea_set_text(ssid_textarea, "");
    }

    if (NULL != password_textarea) {
        lv_textarea_set_text(password_textarea, "");
    }

    if ((NULL != register_keyboard) &&
        (NULL != ssid_textarea)) {
        lv_keyboard_set_textarea(register_keyboard, ssid_textarea);
    }

    state = wifi_connection_get_state();

    switch (state) {
    case WIFI_CONNECTION_CONNECTING:
        gui_wifi_register_set_status("Connecting...");
        break;

    case WIFI_CONNECTION_CONNECTED:
        gui_wifi_register_set_status("Connected");
        break;

    case WIFI_CONNECTION_DISCONNECTING:
        gui_wifi_register_set_status("Disconnecting...");
        break;

    case WIFI_CONNECTION_FAILED:
        gui_wifi_register_set_status("Connection failed");
        break;

    case WIFI_CONNECTION_DISCONNECTED:
    default:
        gui_wifi_register_set_status("Disconnected");
        break;
    }
}

void gui_wifi_register_process(void)
{
    enum wifi_connection_state state = WIFI_CONNECTION_DISCONNECTED;

    if (false == wifi_connection_process()) {
        return;
    }

    state = wifi_connection_get_state();

    switch (state) {
    case WIFI_CONNECTION_DISCONNECTED:
        gui_wifi_register_set_status("Disconnected");
        break;

    case WIFI_CONNECTION_CONNECTING:
        gui_wifi_register_set_status("Connecting...");
        break;

    case WIFI_CONNECTION_CONNECTED:
        gui_wifi_register_set_status("Connected");
        break;

    case WIFI_CONNECTION_DISCONNECTING:
        gui_wifi_register_set_status("Disconnecting...");
        break;

    case WIFI_CONNECTION_FAILED:
    default:
        gui_wifi_register_set_status("Connection failed");
        break;
    }
}
