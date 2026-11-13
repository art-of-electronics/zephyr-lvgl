/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

#include <wifi_connection.h>


/*****************************************************************************
                          PRIVATE DEFINES / MACROS
*****************************************************************************/
#define WIFI_CONNECTION_PSK_MIN_LEN     (8U)
#define WIFI_CONNECTION_PSK_MAX_LEN     (63U)
#define WIFI_CONNECTION_EVENT_QUEUE_LEN (8U)


/*****************************************************************************
                     PRIVATE STRUCTS / ENUMS / VARIABLES
*****************************************************************************/
enum wifi_connection_event_type {
    WIFI_CONNECTION_EVENT_CONNECTED = 0,
    WIFI_CONNECTION_EVENT_DISCONNECTED,
};

struct wifi_connection_event {
    enum wifi_connection_event_type type;
    int status;
};

K_MSGQ_DEFINE(wifi_connection_queue, sizeof(struct wifi_connection_event),
              WIFI_CONNECTION_EVENT_QUEUE_LEN, sizeof(uint32_t));

static struct net_mgmt_event_callback wifi_connection_callback;
static struct net_if *wifi_iface = NULL;

static enum wifi_connection_state connection_state =
    WIFI_CONNECTION_DISCONNECTED;

static bool connection_initialized = false;


/*****************************************************************************
                         PRIVATE FUNCTION DECLARATIONS
*****************************************************************************/

/*****************************************************************************
                       PRIVATE FUNCTION IMPLEMENTATIONS
*****************************************************************************/
static void wifi_connection_event_handler(struct net_mgmt_event_callback *cb,
                                          uint64_t event, struct net_if *iface)
{
    struct wifi_connection_event message = {0};
    const struct wifi_status *status = NULL;

    if (wifi_iface != iface) {
        return;
    }

    if ((NULL == cb->info) || (sizeof(struct wifi_status) > cb->info_length)) {
        return;
    }

    status = (const struct wifi_status *)cb->info;
    message.status = status->status;

    if (NET_EVENT_WIFI_CONNECT_RESULT == event) {
        message.type = WIFI_CONNECTION_EVENT_CONNECTED;
    }
    else if (NET_EVENT_WIFI_DISCONNECT_RESULT == event) {
        message.type = WIFI_CONNECTION_EVENT_DISCONNECTED;
    }
    else {
        return;
    }

    (void)k_msgq_put(&wifi_connection_queue, &message, K_NO_WAIT);
}

bool wifi_connection_process(void)
{
    struct wifi_connection_event message = {0};
    enum wifi_connection_state previous = connection_state;

    while (0 == k_msgq_get(&wifi_connection_queue, &message, K_NO_WAIT)) {
        if (WIFI_CONNECTION_EVENT_CONNECTED == message.type) {
            connection_state = (0 == message.status) ?
                               (WIFI_CONNECTION_CONNECTED) : (WIFI_CONNECTION_FAILED);
        }
        else if (WIFI_CONNECTION_EVENT_DISCONNECTED == message.type) {
            connection_state = (0 == message.status) ?
                               (WIFI_CONNECTION_DISCONNECTED) : (WIFI_CONNECTION_FAILED);
        }
    }

    return (previous != connection_state);
}


/*****************************************************************************
                           INTERFACE IMPLEMENTATION
*****************************************************************************/
int wifi_connection_init(void)
{
    if (true == connection_initialized) {
        return -EALREADY;
    }

    wifi_iface = net_if_get_wifi_sta();
    if (NULL == wifi_iface) {
        return -ENODEV;
    }

    net_mgmt_init_event_callback(&wifi_connection_callback, wifi_connection_event_handler,
                                 NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT);

    net_mgmt_add_event_callback(&wifi_connection_callback);

    connection_state = WIFI_CONNECTION_DISCONNECTED;
    connection_initialized = true;

    return 0;
}

int wifi_connection_connect(const char *ssid, const char *password)
{
    struct wifi_connect_req_params params = {0};
    size_t ssid_length = 0U;
    size_t password_length = 0U;
    int ret = 0;

    if (false == connection_initialized) {
        return -ENODEV;
    }

    if ((NULL == ssid) || (NULL == password)) {
        return -EINVAL;
    }

    if ((WIFI_CONNECTION_CONNECTING == connection_state) ||
        (WIFI_CONNECTION_DISCONNECTING == connection_state)) {
        return -EBUSY;
    }

    if (WIFI_CONNECTION_CONNECTED == connection_state) {
        return -EISCONN;
    }

    ssid_length = strlen(ssid);
    password_length = strlen(password);

    if ((0U == ssid_length) || (WIFI_SSID_MAX_LEN < ssid_length) ||
        (WIFI_CONNECTION_PSK_MIN_LEN > password_length) ||
        (WIFI_CONNECTION_PSK_MAX_LEN < password_length)) {
        return -EINVAL;
    }

    params.ssid = (const uint8_t *)ssid;
    params.ssid_length = (uint8_t)ssid_length;

    params.psk = (const uint8_t *)password;
    params.psk_length = (uint8_t)password_length;

    params.security = WIFI_SECURITY_TYPE_PSK;
    params.channel = WIFI_CHANNEL_ANY;

    ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, wifi_iface, &params, sizeof(params));
    if (0 == ret) {
        connection_state = WIFI_CONNECTION_CONNECTING;
    }
    else {
        connection_state = WIFI_CONNECTION_FAILED;
    }

    return ret;
}

int wifi_connection_disconnect(void)
{
    int ret = 0;

    if (false == connection_initialized) {
        return -ENODEV;
    }

    if (WIFI_CONNECTION_CONNECTED != connection_state) {
        return -ENOTCONN;
    }

    ret = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, wifi_iface, NULL, 0);
    if (0 == ret) {
        connection_state = WIFI_CONNECTION_DISCONNECTING;
    }

    return ret;
}

enum wifi_connection_state wifi_connection_get_state(void)
{
    return connection_state;
}
