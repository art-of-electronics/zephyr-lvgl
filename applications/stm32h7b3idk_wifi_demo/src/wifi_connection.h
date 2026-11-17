/**
 * @file wifi_connection.h
 * @brief Wi-Fi connection management using Zephyr networking APIs.
 *
 * Provides functions for connecting to and disconnecting from
 * wireless networks, monitoring connection states, and processing
 * asynchronous Wi-Fi events.
 */

#ifndef WIFI_CONNECTION_H
#define WIFI_CONNECTION_H

/*****************************************************************************
                                    INCLUDES
*****************************************************************************/
#include <stdbool.h>
#include <stddef.h>


/*****************************************************************************
                       PUBLIC DEFINES / MACROS / ENUMS
*****************************************************************************/
#define WIFI_CONNECTION_MAC_TEXT_SIZE  (18U)
#define WIFI_CONNECTION_IPV4_TEXT_SIZE (16U)

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

/**
 * @brief Get the Wi-Fi interface MAC address.
 * @param [out] buffer Output buffer.
 * @param [out] size Output buffer size.
 * @return 0 on success, negative errno otherwise.
 */
int wifi_connection_get_mac(char *buffer, size_t size);

/**
 * @brief Get the current IPv4 address.
 *
 * @param [out] buffer Output buffer.
 * @param [out] size Output buffer size.
 * @return 0 on success, negative errno otherwise.
 */
int wifi_connection_get_ipv4(char *buffer, size_t size);

#endif // WIFI_CONNECTION_H
