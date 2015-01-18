#include <stdlib.h>
#define _GNU_SOURCE 1
#define __USE_GNU 1
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <libgen.h>
#include <time.h>
#include <sys/time.h>
#include <inttypes.h>

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/afc.h>
#include <libimobiledevice/notification_proxy.h>
#include <libimobiledevice/mobile_image_mounter.h>
#include <asprintf.h>
#include "common/utils.h"

static const char PKG_PATH[] = "PublicStaging";
static const char PATH_PREFIX[] = "/private/var/mobile/Media";

static ssize_t mim_upload_cb(void* buf, size_t size, void* userdata)
{
	ssize_t ret = fread(buf, 1, size, (FILE*)userdata);
	printf("  mim_upload_cb: read %lu bytes, result %lu\n", size, ret);
	return ret;
}

int main(int argc, char **argv)
{
	char *udid = NULL;
	idevice_t device = NULL;
	mobile_image_mounter_client_t mim = NULL;
	char *image_path = NULL;
	char *image_sig_path = NULL;
	char *imagetype = strdup("Developer");

	if (argc <= 1) {
		printf("Usage: %s <dmg>\n", argv[0]);
		return -1;
	}
	image_path = strdup(argv[1]);
	if (asprintf(&image_sig_path, "%s.signature", image_path) < 0) {
		printf("Out of memory?!\n");
		return -1;
	}

	printf("DiskImage: %s\n", image_path);
	printf("Signature: %s\n", image_sig_path);

	if (IDEVICE_E_SUCCESS != idevice_new(&device, udid)) {
		printf("No device found, is it plugged in?\n");
		return -1;
	}

	if (mobile_image_mounter_start_service(device, &mim, "race_condition") != MOBILE_IMAGE_MOUNTER_E_SUCCESS) {
		printf("ERROR: Could not connect to mobile_image_mounter!\n");
		return -1;
	}


	mobile_image_mounter_error_t err;
	size_t image_size = 0;
	char sig[8192] = {0};
	size_t sig_length = 0;

	struct stat fst;
	if (stat(image_path, &fst) != 0) {
		fprintf(stderr, "ERROR: stat: %s: %s\n", image_path, strerror(errno));
		return -2;
	}
	image_size = fst.st_size;

	FILE *fs = fopen(image_sig_path, "rb");
	if (!fs) {
		fprintf(stderr, "Error opening signature file '%s': %s\n", image_sig_path, strerror(errno));
		return -2;
	}
	sig_length = fread(sig, 1, sizeof(sig), fs);
	fclose(fs);
	if (sig_length == 0) {
		fprintf(stderr, "Could not read signature from file '%s'\n", image_sig_path);
		return -2;
	}

	FILE *f = fopen(image_path, "rb");

	printf("Uploading %lu bytes image with %lu sig to device...\n", image_size, sig_length);
	err = mobile_image_mounter_upload_image(mim, imagetype, image_size, sig, sig_length, mim_upload_cb, f);
	if (err != MOBILE_IMAGE_MOUNTER_E_SUCCESS) {
		printf("ERROR: %d\n", (int)err);
	}

	printf("done.\n");

	char *targetname = NULL;
	if (asprintf(&targetname, "%s/%s", PKG_PATH, "staging.dimage") < 0) {
		fprintf(stderr, "Out of memory!?\n");
		return -2;
	}
	char *mountname = NULL;
	if (asprintf(&mountname, "%s/%s", PATH_PREFIX, targetname) < 0) {
		fprintf(stderr, "Out of memory!?\n");
		return -2;
	}

	printf("Mounting: %s\n", mountname);
	
	plist_t result = NULL;
	err = mobile_image_mounter_mount_image(mim, mountname, sig, sig_length, imagetype, &result);
	if (err != MOBILE_IMAGE_MOUNTER_E_SUCCESS) {
		printf("ERROR: %d\n", (int)err);
	}

	mobile_image_mounter_free(mim);
	idevice_free(device);

	if (image_path)
		free(image_path);
	if (image_sig_path)
		free(image_sig_path);
	if (imagetype)
		free(imagetype);

	return 0;
}
