/*
 * pcap.c 
 * com.apple.pcapd service implementation.
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

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <plist/plist.h>

#include "pcap_relay.h"
#include "property_list_service.h"
#include "common/debug.h"

/**
 * Convert a service_error_t value to a pcap_relay_error_t value.
 * Used internally to get correct error codes.
 *
 * @param err An service_error_t error code
 *
 * @return A matching pcap_relay_error_t error code,
 *     PCAP_RELAY_E_UNKNOWN_ERROR otherwise.
 */
static pcap_relay_error_t pcap_relay_error(property_list_service_error_t err)
{
	switch (err) {
		case PROPERTY_LIST_SERVICE_E_SUCCESS:
			return PCAP_RELAY_E_SUCCESS;
		case PROPERTY_LIST_SERVICE_E_INVALID_ARG:
			return PCAP_RELAY_E_INVALID_ARG;
		case PROPERTY_LIST_SERVICE_E_PLIST_ERROR:
			return PCAP_RELAY_E_PLIST_ERROR;
		case PROPERTY_LIST_SERVICE_E_MUX_ERROR:
			return PCAP_RELAY_E_CONN_FAILED;
		default:
			break;
	}
	return PCAP_RELAY_E_UNKNOWN_ERROR;
}

LIBIMOBILEDEVICE_API pcap_relay_error_t pcap_relay_client_new(idevice_t device, lockdownd_service_descriptor_t service, pcap_relay_client_t *client)
{
	property_list_service_client_t plistclient = NULL;
	pcap_relay_error_t err = pcap_relay_error(property_list_service_client_new(device, service, &plistclient));
	if (err != PCAP_RELAY_E_SUCCESS) {
		return err;
	}

	pcap_relay_client_t client_loc = (pcap_relay_client_t) malloc(sizeof(struct pcap_relay_client_private));
	client_loc->parent = plistclient;

	mutex_init(&client_loc->mutex);

	*client = client_loc;
	return PCAP_RELAY_E_SUCCESS;
}

LIBIMOBILEDEVICE_API pcap_relay_error_t pcap_relay_client_start_service(idevice_t device, pcap_relay_client_t * client, const char* label)
{
	pcap_relay_error_t err = PCAP_RELAY_E_UNKNOWN_ERROR;
	service_client_factory_start_service(device, PCAP_RELAY_SERVICE_NAME, (void**)client, label, SERVICE_CONSTRUCTOR(pcap_relay_client_new), &err);
	return err;
}

LIBIMOBILEDEVICE_API pcap_relay_error_t pcap_relay_client_free(pcap_relay_client_t client)
{
	if (!client)
		return PCAP_RELAY_E_INVALID_ARG;

	property_list_service_client_free(client->parent);
	client->parent = NULL;
	mutex_destroy(&client->mutex);
	free(client);

	return PCAP_RELAY_E_SUCCESS;
}

/**
 * Receives a plist from the service.
 *
 * @param client The pcap_relay client
 * @param plist The plist to store the received data
 *
 * @return PCAP_RELAY_E_SUCCESS on success,
 *  PCAP_RELAY_E_INVALID_ARG when client or plist is NULL
 */
static pcap_relay_error_t pcap_relay_receive(pcap_relay_client_t client, plist_t *plist)
{
	if (!client || !plist || (plist && *plist))
		return PCAP_RELAY_E_INVALID_ARG;

	pcap_relay_error_t ret = PCAP_RELAY_E_SUCCESS;
	property_list_service_error_t err;

	err = property_list_service_receive_plist(client->parent, plist);
	if (err != PROPERTY_LIST_SERVICE_E_SUCCESS) {
		ret = PCAP_RELAY_E_UNKNOWN_ERROR;
	}

	if (!*plist)
		ret = PCAP_RELAY_E_PLIST_ERROR;

	return ret;
}

LIBIMOBILEDEVICE_API pcap_relay_error_t pcap_relay_receive_buffer(pcap_relay_client_t client, char **buffer, uint64_t *bytes_receive)
{
	if (!client || buffer == NULL || bytes_receive == NULL)
		return PCAP_RELAY_E_INVALID_ARG;

	pcap_relay_error_t ret = PCAP_RELAY_E_UNKNOWN_ERROR;

	plist_t result = NULL;
	ret = pcap_relay_receive(client, &result);
	if (!result) {
		return PCAP_RELAY_E_PLIST_ERROR;
	}

	if (plist_get_node_type(result) != PLIST_DATA) {
		fprintf(stderr, "%s: unexpected plist node type for result (PLIST_DATA expected but %d received)\n", __func__, plist_get_node_type(result));
		return PCAP_RELAY_E_PLIST_ERROR;
	}

	plist_get_data_val(result, buffer, bytes_receive);
	if (!*buffer) {
		fprintf(stderr, "%s: could not get data value from plist node\n", __func__);
		return PCAP_RELAY_E_PLIST_ERROR;
	}

	plist_free(result);
	return ret;
}
