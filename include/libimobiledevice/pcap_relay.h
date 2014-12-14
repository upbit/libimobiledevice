/**
 * @file libimobiledevice/pcap_relay.h
 * @brief Communicate with pcapd on the device.
 * \internal
 *
 * Copyright (c) 2014 Martin Szulecki All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef IPCAP_RELAY_H
#define IPCAP_RELAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

#define PCAP_RELAY_SERVICE_NAME "com.apple.pcapd"

/** Error Codes */
typedef enum {
	PCAP_RELAY_E_SUCCESS        =  0,
	PCAP_RELAY_E_INVALID_ARG    = -1,
	PCAP_RELAY_E_PLIST_ERROR    = -2,
	PCAP_RELAY_E_CONN_FAILED    = -3,
	PCAP_RELAY_E_UNKNOWN_ERROR  = -256
} pcap_relay_error_t;

typedef struct pcap_relay_client_private pcap_relay_client_private;
typedef pcap_relay_client_private *pcap_relay_client_t; /**< The client handle. */


/* Interface */

/**
 * Connects to the pcapd service on the specified device.
 *
 * @param device The device to connect to.
 * @param service The service descriptor returned by lockdownd_start_service.
 * @param client Pointer that will point to a newly allocated
 *     pcap_relay_client_t upon successful return. Must be freed using
 *     pcap_relay_client_free() after use.
 *
 * @return PCAP_RELAY_E_SUCCESS on success, PCAP_RELAY_E_INVALID_ARG when
 *     client is NULL, or an PCAP_RELAY_E_* error code otherwise.
 */
pcap_relay_error_t pcap_relay_client_new(idevice_t device, lockdownd_service_descriptor_t service, pcap_relay_client_t * client);

/**
 * Starts a new pcapd service on the specified device and connects to it.
 *
 * @param device The device to connect to.
 * @param client Pointer that will point to a newly allocated
 *     pcap_relay_client_t upon successful return. Must be freed using
 *     pcap_relay_client_free() after use.
 * @param label The label to use for communication. Usually the program name.
 *  Pass NULL to disable sending the label in requests to lockdownd.
 *
 * @return PCAP_RELAY_E_SUCCESS on success, or an pcap_E_* error
 *     code otherwise.
 */
pcap_relay_error_t pcap_relay_client_start_service(idevice_t device, pcap_relay_client_t * client, const char* label);

/**
 * Disconnects a pcapd client from the device and frees up the
 * pcapd client data.
 *
 * @param client The pcapd client to disconnect and free.
 *
 * @return PCAP_RELAY_E_SUCCESS on success, PCAP_RELAY_E_INVALID_ARG when
 *     client is NULL, or an PCAP_RELAY_E_* error code otherwise.
 */
pcap_relay_error_t pcap_relay_client_free(pcap_relay_client_t client);


pcap_relay_error_t pcap_relay_receive_buffer(pcap_relay_client_t client, char **buffer, uint64_t *bytes_receive);


#ifdef __cplusplus
}
#endif

#endif
