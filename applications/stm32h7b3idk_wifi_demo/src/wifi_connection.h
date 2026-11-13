#ifndef WIFI_CONNECTION_H_
#define WIFI_CONNECTION_H_

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <stdbool.h>


/*****************************************************************************
                       PUBLIC DEFINES / MACROS / ENUMS
*****************************************************************************/
/** @brief Wi-Fi connection states exposed to the application. */
enum wifi_connection_state {
    WIFI_CONNECTION_DISCONNECTED = 0,
    WIFI_CONNECTION_CONNECTING,
    WIFI_CONNECTION_CONNECTED,
    WIFI_CONNECTION_DISCONNECTING,
    WIFI_CONNECTION_FAILED,
};


/*****************************************************************************
                         PUBLIC INTERFACE DECLARATION
*****************************************************************************/
/**
 * @brief Initialize the Wi-Fi connection manager.
 * @return 0 on success, negative errno otherwise.
 */
int wifi_connection_init(void);

/**
 * @brief Request a WPA/WPA2-Personal connection.
 * @param [in] ssid Null-terminated SSID (1..32 bytes).
 * @param [in] password Null-terminated passphrase (8..63 bytes).
 * @return 0 if accepted, negative errno otherwise.
 */
int wifi_connection_connect(const char *ssid, const char *password);

/**
 * @brief Request a Wi-Fi disconnection.
 * @return 0 if accepted, negative errno otherwise.
 */
int wifi_connection_disconnect(void);

/**
 * @brief Process queued network events.
 * @return true if connection state changed.
 * @note Must be called from the application/GUI thread.
 */
bool wifi_connection_process(void);

/** 
 * @brief Return the current connection state. 
 */
enum wifi_connection_state wifi_connection_get_state(void);

#endif // WIFI_CONNECTION_H_
