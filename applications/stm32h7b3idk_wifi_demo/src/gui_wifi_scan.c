/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

#include <lvgl.h>
#include <theme.h>
#include <gui_wifi_scan.h>


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/

#define WIFI_SCAN_MAX_RESULTS       (20U)
#define WIFI_SCAN_QUEUE_SIZE        (WIFI_SCAN_MAX_RESULTS + 1U)

#define WIFI_SCAN_TABLE_WIDTH       (450)
#define WIFI_SCAN_TABLE_HEIGHT      (157)
#define WIFI_SCAN_TABLE_Y           (78)
#define WIFI_SCAN_HEADER_HEIGHT     (23)
#define WIFI_SCAN_HEADER_Y          (55)

#define WIFI_SCAN_COL_NUM           (40)
#define WIFI_SCAN_COL_SSID          (245)
#define WIFI_SCAN_COL_BAND          (90)
#define WIFI_SCAN_COL_RSSI          (75)
#define WIFI_CHANNEL_2_4_MIN        (1U)
#define WIFI_CHANNEL_2_4_MAX        (14U)

#define WIFI_SCAN_CELL_PAD_H        (5)
#define WIFI_SCAN_CELL_PAD_V        (4)
#define WIFI_SCAN_ROW_HEIGHT        (32)
#define WIFI_SCAN_ROW_SEPARATOR_X   (12)
#define WIFI_SCAN_ROW_SEPARATOR_H   (1)
#define WIFI_SCAN_ROW_SEPARATOR_W   (WIFI_SCAN_TABLE_WIDTH - (2 * WIFI_SCAN_ROW_SEPARATOR_X))
#define WIFI_SCAN_TEXT_Y            (6)

#define WIFI_SCAN_NUMBER_TEXT_SIZE  (8U)
#define WIFI_SCAN_RSSI_TEXT_SIZE    (8U)
#define WIFI_SCAN_STATUS_TEXT_SIZE  (40U)

#define WIFI_STATUS_OFFSET_X      (12)
#define WIFI_STATUS_OFFSET_Y      (-5)

#define WIFI_SCAN_REFRESH_WIDTH         (90)
#define WIFI_SCAN_REFRESH_HEIGHT        (24)
#define WIFI_SCAN_REFRESH_OFFSET_X      (-10)
#define WIFI_SCAN_REFRESH_OFFSET_Y      (-2)


/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/
enum wifi_scan_message_type {
    WIFI_SCAN_MESSAGE_RESULT,
    WIFI_SCAN_MESSAGE_DONE
};

struct wifi_scan_message {
    enum wifi_scan_message_type type;
    char ssid[WIFI_SSID_MAX_LEN + 1U];
    uint8_t channel;
    int8_t rssi;
    int status;
};

K_MSGQ_DEFINE(wifi_scan_queue, sizeof(struct wifi_scan_message),
              WIFI_SCAN_QUEUE_SIZE, sizeof(uint32_t));

static struct net_mgmt_event_callback wifi_scan_callback;

static lv_obj_t *scan_list = NULL;
static lv_obj_t *scan_status = NULL;
static lv_obj_t *refresh_button = NULL;
static lv_obj_t *scan_screen = NULL;

static uint32_t scan_count = 0U;
static bool scan_in_progress = false;
static bool scan_visible = false;

/*****************************************************************************
                         PRIVATE FUNCTION DECLARATIONS
*****************************************************************************/

/*****************************************************************************
                       PRIVATE FUNCTION IMPLEMENTATIONS
*****************************************************************************/
static void wifi_gui_scan_event_handler(struct net_mgmt_event_callback *cb,
                                        uint64_t event, struct net_if *iface)
{
    struct wifi_scan_message message = {0};

    (void)iface;

    if (NET_EVENT_WIFI_SCAN_RESULT == event) {
        const struct wifi_scan_result *result = NULL;
        size_t ssid_length = 0U;

        if ((NULL == cb->info) || (sizeof(struct wifi_scan_result) > cb->info_length)) {
            return;
        }

        if (WIFI_SCAN_MAX_RESULTS <= k_msgq_num_used_get(&wifi_scan_queue)) {
            return;
        }

        result = (const struct wifi_scan_result *)cb->info;

        ssid_length = (WIFI_SSID_MAX_LEN < result->ssid_length) 
                       ? (WIFI_SSID_MAX_LEN) : (result->ssid_length);

        message.type = WIFI_SCAN_MESSAGE_RESULT;
        message.channel = result->channel;
        message.rssi = result->rssi;

        memcpy(message.ssid, result->ssid, ssid_length);
        message.ssid[ssid_length] = '\0';

        (void)k_msgq_put(&wifi_scan_queue, &message, K_NO_WAIT);
    }
    else if (NET_EVENT_WIFI_SCAN_DONE == event) {
        message.type = WIFI_SCAN_MESSAGE_DONE;

        if ((NULL != cb->info) && (sizeof(struct wifi_status) <= cb->info_length)) {
            const struct wifi_status *status = (const struct wifi_status *)cb->info;
            message.status = status->status;
        }

        (void)k_msgq_put(&wifi_scan_queue, &message, K_NO_WAIT);
    }
}

static lv_obj_t *wifi_gui_scan_create_cell(lv_obj_t *row, const char *text, 
                                           int32_t x, int32_t width)
{
    lv_obj_t *label = NULL;

    label = lv_label_create(row);
    if (NULL == label) {
        return NULL;
    }

    lv_obj_set_width(label, width - (2 * WIFI_SCAN_CELL_PAD_H));
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(SAMUS_BLUE), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(label, x + WIFI_SCAN_CELL_PAD_H, WIFI_SCAN_TEXT_Y);
    lv_obj_set_clickable(label, false);

    return label;
}

static void wifi_gui_scan_add_result(const struct wifi_scan_message *message)
{
    lv_obj_t *row = NULL;
    lv_obj_t *separator = NULL;
    char number[WIFI_SCAN_NUMBER_TEXT_SIZE] = {0};
    char rssi[WIFI_SCAN_RSSI_TEXT_SIZE] = {0};
    const char *band_text = "--";

    if ((NULL == message) || (NULL == scan_list) ||
        (WIFI_SCAN_MAX_RESULTS <= scan_count)) {
        return;
    }

    row = lv_obj_create(scan_list);
    if (NULL == row) {
        return;
    }

    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, WIFI_SCAN_TABLE_WIDTH, WIFI_SCAN_ROW_HEIGHT);
    lv_obj_set_scrollable(row, false);
    lv_obj_set_clickable(row, false);

    (void)snprintf(number, sizeof(number), "%u", (unsigned int)(scan_count + 1U));
    (void)snprintf(rssi, sizeof(rssi), "%d", (int)message->rssi);
    
    if ((WIFI_CHANNEL_2_4_MIN <= message->channel) &&
        (WIFI_CHANNEL_2_4_MAX >= message->channel)) {
        band_text = "2.4GHz";
    }
    else if (WIFI_CHANNEL_2_4_MAX < message->channel) {
        band_text = "5GHz";
    }

    if ((NULL == wifi_gui_scan_create_cell(row, number, 0, WIFI_SCAN_COL_NUM)) ||
        (NULL == wifi_gui_scan_create_cell(row, ('\0' != message->ssid[0]) 
                                                ? (message->ssid) : ("<hidden>"),
                                           WIFI_SCAN_COL_NUM, WIFI_SCAN_COL_SSID)) ||
        (NULL == wifi_gui_scan_create_cell(row, band_text, 
                                           WIFI_SCAN_COL_NUM + WIFI_SCAN_COL_SSID, 
                                           WIFI_SCAN_COL_BAND)) ||
        (NULL == wifi_gui_scan_create_cell(row, rssi,
                                           WIFI_SCAN_COL_NUM + WIFI_SCAN_COL_SSID + 
                                           WIFI_SCAN_COL_BAND,
                                           WIFI_SCAN_COL_RSSI))) {
        lv_obj_delete(row);
        return;
    }

    separator = lv_obj_create(row);
    if (NULL == separator) {
        lv_obj_delete(row);
        return;
    }

    lv_obj_remove_style_all(separator);
    lv_obj_set_size(separator, WIFI_SCAN_ROW_SEPARATOR_W, WIFI_SCAN_ROW_SEPARATOR_H);
    lv_obj_set_pos(separator, WIFI_SCAN_ROW_SEPARATOR_X, 
                   WIFI_SCAN_ROW_HEIGHT - WIFI_SCAN_ROW_SEPARATOR_H);
    lv_obj_set_style_bg_color(separator, lv_color_hex(SAMUS_GREEN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(separator, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_clickable(separator, false);

    scan_count++;
}

static void wifi_gui_refresh(lv_event_t *event)
{
    (void)event;
    gui_wifi_scan_start();
}

static void wifi_gui_scan_style_list(lv_obj_t *list, lv_color_t accent)
{
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, WIFI_SCAN_TABLE_WIDTH, WIFI_SCAN_TABLE_HEIGHT);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, WIFI_SCAN_TABLE_Y);
    lv_obj_set_style_bg_color(list, lv_color_hex(CHOZO_DARK_BLUE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(list, accent, LV_PART_MAIN);
    lv_obj_set_style_border_side(list,
                                 LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT |
                                 LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(list, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_clickable(list, true);
}

static int wifi_gui_scan_create_header(lv_color_t accent)
{
    static const char *const headings[] = {"#", "SSID", "Band", "RSSI"};
    static const int32_t widths[] = {
        WIFI_SCAN_COL_NUM, WIFI_SCAN_COL_SSID,
        WIFI_SCAN_COL_BAND, WIFI_SCAN_COL_RSSI
    };
    lv_obj_t *header = NULL;
    lv_obj_t *label = NULL;
    int32_t x = 0;

    header = lv_obj_create(scan_screen);
    if (NULL == header) {
        return -ENOMEM;
    }

    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, WIFI_SCAN_TABLE_WIDTH, WIFI_SCAN_HEADER_HEIGHT);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, WIFI_SCAN_HEADER_Y);
    lv_obj_set_scrollable(header, false);
    lv_obj_set_clickable(header, false);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(header, accent, LV_PART_MAIN);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);

    for (uint32_t col = 0U; col < 4U; col++) {
        label = lv_label_create(header);
        if (NULL == label) {
            lv_obj_delete(header);
            return -ENOMEM;
        }

        lv_label_set_text(label, headings[col]);
        lv_obj_set_style_text_color(label, accent, LV_PART_MAIN);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_pos(label, x + WIFI_SCAN_CELL_PAD_H, 2);
        x += widths[col];
    }

    return 0;
}


/*****************************************************************************
                           INTERFACE IMPLEMENTATION
*****************************************************************************/
int gui_wifi_scan_init(lv_obj_t *screen, lv_color_t accent)
{
    lv_obj_t *label = NULL;
    int ret = 0;

    if (NULL == screen) {
        return -EINVAL;
    }
    if (NULL != scan_screen) {
        return -EALREADY;
    }

    scan_screen = screen;

    refresh_button = lv_button_create(scan_screen);
    if (NULL == refresh_button) {
        return -ENOMEM;
    }

    lv_obj_set_size(refresh_button, WIFI_SCAN_REFRESH_WIDTH, WIFI_SCAN_REFRESH_HEIGHT);
    lv_obj_align(refresh_button, LV_ALIGN_BOTTOM_RIGHT, WIFI_SCAN_REFRESH_OFFSET_X,
                 WIFI_SCAN_REFRESH_OFFSET_Y);
    lv_obj_set_style_bg_opa(refresh_button, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(refresh_button, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(refresh_button, 0, LV_PART_MAIN);

    label = lv_label_create(refresh_button);
    if (NULL == label) {
        return -ENOMEM;
    }
    lv_label_set_text(label, "Refresh");
    lv_obj_set_style_text_color(label, accent, LV_PART_MAIN);
    lv_obj_center(label);
    lv_obj_add_event_cb(refresh_button, wifi_gui_refresh, LV_EVENT_CLICKED, NULL);

    ret = wifi_gui_scan_create_header(accent);
    if (0 != ret) {
        return ret;
    }

    scan_list = lv_obj_create(scan_screen);
    if (NULL == scan_list) {
        return -ENOMEM;
    }

    wifi_gui_scan_style_list(scan_list, accent);

    scan_status = lv_label_create(scan_screen);
    if (NULL == scan_status) {
        return -ENOMEM;
    }
    
    lv_label_set_text(scan_status, "Ready");
    lv_obj_set_style_text_color(scan_status, accent, LV_PART_MAIN);
    lv_obj_align(scan_status, LV_ALIGN_BOTTOM_LEFT, WIFI_STATUS_OFFSET_X, WIFI_STATUS_OFFSET_Y);

    net_mgmt_init_event_callback(&wifi_scan_callback, wifi_gui_scan_event_handler,
                                 NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE);
    net_mgmt_add_event_callback(&wifi_scan_callback);

    return 0;
}

void gui_wifi_scan_start(void)
{
    struct net_if *iface = NULL;
    char status_text[WIFI_SCAN_STATUS_TEXT_SIZE] = {0};
    int ret = 0;

    if ((NULL == scan_list) || (NULL == scan_status) || (NULL == refresh_button)) {
        return;
    }

    scan_visible = true;

    if (true == scan_in_progress) {
        lv_label_set_text(scan_status, "Scanning...");
        lv_obj_set_clickable(refresh_button, false);
        return;
    }

    iface = net_if_get_wifi_sta();
    if (NULL == iface) {
        lv_label_set_text(scan_status, "Wi-Fi interface unavailable");
        return;
    }

    k_msgq_purge(&wifi_scan_queue);

    scan_count = 0U;
    scan_in_progress = true;

    lv_obj_clean(scan_list);
    lv_obj_scroll_to_y(scan_list, 0, LV_ANIM_OFF);
    lv_label_set_text(scan_status, "Scanning...");
    lv_obj_set_clickable(refresh_button, false);

    ret = net_mgmt(NET_REQUEST_WIFI_SCAN, iface, NULL, 0);
    if (0 != ret) {
        scan_in_progress = false;
        lv_obj_set_clickable(refresh_button, true);

        (void)snprintf(status_text, sizeof(status_text), "Scan failed: %d", ret);
        lv_label_set_text(scan_status, status_text);
    }
}

void gui_wifi_scan_process(void)
{
    struct wifi_scan_message message = {0};
    char status_text[WIFI_SCAN_STATUS_TEXT_SIZE] = {0};

    while (0 == k_msgq_get(&wifi_scan_queue, &message, K_NO_WAIT)) {
        if (WIFI_SCAN_MESSAGE_RESULT == message.type) {
            if (true == scan_visible) {
                wifi_gui_scan_add_result(&message);
            }
        }
        else if (WIFI_SCAN_MESSAGE_DONE == message.type) {
            scan_in_progress = false;

            if (true == scan_visible) {
                lv_obj_set_clickable(refresh_button, true);

                if (0 != message.status) {
                    (void)snprintf(status_text, sizeof(status_text), 
                                   "Scan failed: %d", message.status);
                }
                else {
                    (void)snprintf(status_text, sizeof(status_text),
                                   "Found %u networks", (unsigned int)scan_count);
                }

                lv_label_set_text(scan_status, status_text);
            }
        }
    }
}

void gui_wifi_scan_close(void)
{
    scan_visible = false;
    scan_count = 0U;

    if (NULL != scan_list) {
        lv_obj_clean(scan_list);
        lv_obj_scroll_to_y(scan_list, 0, LV_ANIM_OFF);
    }

    if (NULL != scan_status) {
        lv_label_set_text(scan_status, "Ready");
    }

    if (NULL != refresh_button) {
        lv_obj_set_clickable(refresh_button, (false == scan_in_progress));
    }
}
