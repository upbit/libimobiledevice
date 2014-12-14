/*
 * idevicepcap.c
 * Command line interface to diagnostic packet capture from an iOS device
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/pcap_relay.h>

void hexdump(void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.
            printf ("  %04X: ", i);
        }

        // Now the hex code for the specific character.
        printf (" %02X", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf ("  %s\n", buff);
}

int main(int argc, char *argv[])
{
    printf("Just use `rvictl -s` instead this tools.\n");

	char *udid = NULL;
	idevice_t device = NULL;
	pcap_relay_client_t pcap_relay = NULL;

	if (IDEVICE_E_SUCCESS != idevice_new(&device, udid)) {
		printf("No device found, is it plugged in?\n");
		return -1;
	}

	if (pcap_relay_client_start_service(device, &pcap_relay, "idevicerun") != PCAP_RELAY_E_SUCCESS) {
		fprintf(stderr, "Could not start pcapd service.\n");
		return -2;
	}

    printf(">> start capture packet:\n");

	for (int i = 0; i < 3; i++) {
		char* buff = NULL;
		uint64_t length = 0;

        pcap_relay_receive_buffer(pcap_relay, &buff, &length);

		printf(">>> recv %llu bytes from pcapd:\n", length);
		hexdump(buff, length);

		free(buff);
	}

	pcap_relay_client_free(pcap_relay);
	idevice_free(device);
	return 0;
}
