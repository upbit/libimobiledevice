/*
 * idevicesyslog.c
 * Relay the syslog of a device to stdout
 *
 * Copyright (c) 2009 Martin Szulecki All Rights Reserved.
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

#ifdef WIN32
#include <windows.h>
#define sleep(x) Sleep(x*1000)
#endif

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/syslog_relay.h>

static int quit_flag = 0;

void print_usage(int argc, char **argv);

static char* udid = NULL;

static idevice_t device = NULL;
static syslog_relay_client_t syslog = NULL;

// Color syslog form rpetrich/deviceconsole
// https://github.com/rpetrich/deviceconsole/blob/master/main.c
#define MAX_LINEBUFFER_SIZE (4096)
static int line_buffer_offset = 0;
static char line_buffer[MAX_LINEBUFFER_SIZE+1] = {0};

#define COLOR_RESET         "\e[m"
#define COLOR_NORMAL        "\e[0m"
#define COLOR_DARK          "\e[2m"
#define COLOR_RED           "\e[0;31m"
#define COLOR_DARK_RED      "\e[2;31m"
#define COLOR_GREEN         "\e[0;32m"
#define COLOR_DARK_GREEN    "\e[2;32m"
#define COLOR_YELLOW        "\e[0;33m"
#define COLOR_DARK_YELLOW   "\e[2;33m"
#define COLOR_BLUE          "\e[0;34m"
#define COLOR_DARK_BLUE     "\e[2;34m"
#define COLOR_MAGENTA       "\e[0;35m"
#define COLOR_DARK_MAGENTA  "\e[2;35m"
#define COLOR_CYAN          "\e[0;36m"
#define COLOR_DARK_CYAN     "\e[2;36m"
#define COLOR_WHITE         "\e[0;37m"
#define COLOR_DARK_WHITE    "\e[0;37m"

static inline void write_fully(int fd, const char *buffer, size_t length)
{
    while (length) {
        ssize_t result = write(fd, buffer, length);
        if (result == -1)
            break;
        buffer += result;
        length -= result;
    }
}

static inline void write_string(int fd, const char *string)
{
    write_fully(fd, string, strlen(string));
}

static int find_space_offsets(const char *buffer, size_t length, size_t *space_offsets_out)
{
    int o = 0;
    for (size_t i = 16; i < length; i++) {
        if (buffer[i] == ' ') {
            space_offsets_out[o++] = i;
            if (o == 3) {
                break;
            }
        }
    }
    
    return o;
}

#define write_const(fd, text) write_fully(fd, text, sizeof(text)-1)

static void write_colored(int fd, const char *buffer, size_t length)
{
    if (length < 16) {
        write_fully(fd, buffer, length);
        return;
    }
    
    size_t space_offsets[3];
    int o = find_space_offsets(buffer, length, space_offsets);
    
    if (o == 3) {
        
        // Log date and device name
        write_const(fd, COLOR_DARK_WHITE);
        write_fully(fd, buffer, space_offsets[0]);
        // Log process name
        int pos = 0;
        for (int i = space_offsets[0]; i < (int)space_offsets[0]; i++) {
            if (buffer[i] == '[') {
                pos = i;
                break;
            }
        }
        write_const(fd, COLOR_CYAN);
        if (pos && buffer[space_offsets[1]-1] == ']') {
            write_fully(fd, buffer + space_offsets[0], pos - space_offsets[0]);
            write_const(fd, COLOR_DARK_CYAN);
            write_fully(fd, buffer + pos, space_offsets[1] - pos);
        } else {
            write_fully(fd, buffer + space_offsets[0], space_offsets[1] - space_offsets[0]);
        }
        // Log level
        size_t levelLength = space_offsets[2] - space_offsets[1];
        if (levelLength > 4) {
            const char *normalColor;
            const char *darkColor;
            if (levelLength == 9 && memcmp(buffer + space_offsets[1], " <Debug>:", 9) == 0){
                normalColor = COLOR_MAGENTA;
                darkColor = COLOR_DARK_MAGENTA;
            } else if (levelLength == 11 && memcmp(buffer + space_offsets[1], " <Warning>:", 11) == 0){
                normalColor = COLOR_YELLOW;
                darkColor = COLOR_DARK_YELLOW;
            } else if (levelLength == 9 && memcmp(buffer + space_offsets[1], " <Error>:", 9) == 0){
                normalColor = COLOR_RED;
                darkColor = COLOR_DARK_RED;
            } else if (levelLength == 10 && memcmp(buffer + space_offsets[1], " <Notice>:", 10) == 0) {
                normalColor = COLOR_GREEN;
                darkColor = COLOR_DARK_GREEN;
            } else {
                goto level_unformatted;
            }
            write_string(fd, darkColor);
            write_fully(fd, buffer + space_offsets[1], 2);
            write_string(fd, normalColor);
            write_fully(fd, buffer + space_offsets[1] + 2, levelLength - 4);
            write_string(fd, darkColor);
            write_fully(fd, buffer + space_offsets[1] + levelLength - 2, 1);
            write_const(fd, COLOR_DARK_WHITE);
            write_fully(fd, buffer + space_offsets[1] + levelLength - 1, 1);
        } else {
        level_unformatted:
            write_const(fd, COLOR_RESET);
            write_fully(fd, buffer + space_offsets[1], levelLength);
        }
        write_const(fd, COLOR_RESET);
        write_fully(fd, buffer + space_offsets[2], length - space_offsets[2]);
    } else {
        write_fully(fd, buffer, length);
    }
}

static void line_buffer_add(char c)
{
    line_buffer[line_buffer_offset++] = c;
    if ((c == '\n') || (line_buffer_offset >= MAX_LINEBUFFER_SIZE)) {
        write_colored(fileno(stdout), line_buffer, line_buffer_offset);
        line_buffer_offset = 0;
    }
}

static void syslog_callback(char c, void *user_data)
{
	//putchar(c);
    line_buffer_add(c);
}

static int start_logging(void)
{
	idevice_error_t ret = idevice_new(&device, udid);
	if (ret != IDEVICE_E_SUCCESS) {
		fprintf(stderr, "Device with udid %s not found!?\n", udid);
		return -1;
	}

	/* start and connect to syslog_relay service */
	syslog_relay_error_t serr = SYSLOG_RELAY_E_UNKNOWN_ERROR;
	serr = syslog_relay_client_start_service(device, &syslog, "idevicesyslog");
	if (serr != SYSLOG_RELAY_E_SUCCESS) {
		fprintf(stderr, "ERROR: Could not start service com.apple.syslog_relay.\n");
		idevice_free(device);
		device = NULL;
		return -1;
	}

	/* start capturing syslog */
	serr = syslog_relay_start_capture(syslog, syslog_callback, NULL);
	if (serr != SYSLOG_RELAY_E_SUCCESS) {
		fprintf(stderr, "ERROR: Unable tot start capturing syslog.\n");
		syslog_relay_client_free(syslog);
		syslog = NULL;
		idevice_free(device);
		device = NULL;
		return -1;
	}

	fprintf(stdout, "[connected]\n");
	fflush(stdout);

	return 0;
}

static void stop_logging(void)
{
	fflush(stdout);

	if (syslog) {
		syslog_relay_client_free(syslog);
		syslog = NULL;
	}

	if (device) {
		idevice_free(device);
		device = NULL;
	}
}

static void device_event_cb(const idevice_event_t* event, void* userdata)
{
	if (event->event == IDEVICE_DEVICE_ADD) {
		if (!syslog) {
			if (!udid) {
				udid = strdup(event->udid);
			}
			if (strcmp(udid, event->udid) == 0) {
				if (start_logging() != 0) {
					fprintf(stderr, "Could not start logger for udid %s\n", udid);
				}
			}
		}
	} else if (event->event == IDEVICE_DEVICE_REMOVE) {
		if (syslog && (strcmp(udid, event->udid) == 0)) {
			stop_logging();
			fprintf(stdout, "[disconnected]\n");
		}
	}
}

/**
 * signal handler function for cleaning up properly
 */
static void clean_exit(int sig)
{
	fprintf(stderr, "\nExiting...\n");
	quit_flag++;
}

int main(int argc, char *argv[])
{
	int i;

	signal(SIGINT, clean_exit);
	signal(SIGTERM, clean_exit);
#ifndef WIN32
	signal(SIGQUIT, clean_exit);
	signal(SIGPIPE, SIG_IGN);
#endif

	/* parse cmdline args */
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")) {
			idevice_set_debug_level(1);
			continue;
		}
		else if (!strcmp(argv[i], "-u") || !strcmp(argv[i], "--udid")) {
			i++;
			if (!argv[i] || (strlen(argv[i]) != 40)) {
				print_usage(argc, argv);
				return 0;
			}
			udid = strdup(argv[i]);
			continue;
		}
		else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			print_usage(argc, argv);
			return 0;
		}
		else {
			print_usage(argc, argv);
			return 0;
		}
	}

	int num = 0;
	char **devices = NULL;
	idevice_get_device_list(&devices, &num);
	idevice_device_list_free(devices);
	if (num == 0) {
		if (!udid) {
			fprintf(stderr, "No device found. Plug in a device or pass UDID with -u to wait for device to be available.\n");
			return -1;
		} else {
			fprintf(stderr, "Waiting for device with UDID %s to become available...\n", udid);
		}
	}

	idevice_event_subscribe(device_event_cb, NULL);

	while (!quit_flag) {
		sleep(1);
	}
	idevice_event_unsubscribe();
	stop_logging();
 
	if (udid) {
		free(udid);
	}

	return 0;
}

void print_usage(int argc, char **argv)
{
	char *name = NULL;
	
	name = strrchr(argv[0], '/');
	printf("Usage: %s [OPTIONS]\n", (name ? name + 1: argv[0]));
	printf("Relay syslog of a connected device.\n\n");
	printf("  -d, --debug\t\tenable communication debugging\n");
	printf("  -u, --udid UDID\ttarget specific device by its 40-digit device UDID\n");
	printf("  -h, --help\t\tprints usage information\n");
	printf("\n");
}

