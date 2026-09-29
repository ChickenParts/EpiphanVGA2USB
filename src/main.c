#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bootstrap.h"
#include "firmware.h"

static int control_out(void *context, uint8_t request, uint16_t value,
                       uint16_t index, const uint8_t *data, uint16_t length)
{
    libusb_device_handle *handle = context;
    /* USB timeout zero means no deadline, per ChickenParts project conventions. */
    return libusb_control_transfer(handle, 0x40, request, value, index,
                                   (unsigned char *)data, length, 0);
}

static void physical_path(libusb_device *device, char *out, size_t size)
{
    uint8_t ports[8];
    int count = libusb_get_port_numbers(device, ports, sizeof ports);
    int used = snprintf(out, size, "%u", libusb_get_bus_number(device));
    for (int i = 0; i < count && used >= 0 && (size_t)used < size; ++i) {
        int n = snprintf(out + used, size - (size_t)used, "%s%u", i ? "." : "-", ports[i]);
        if (n < 0) break;
        used += n;
    }
}

int main(int argc, char **argv)
{
    const char *action = "--inspect", *port = NULL;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--port") && i + 1 < argc) port = argv[++i];
        else if (!strcmp(argv[i], "--inspect") || !strcmp(argv[i], "--bootstrap") || !strcmp(argv[i], "--sync")) action = argv[i];
        else {
            fprintf(stderr, "Usage: %s [--inspect|--bootstrap|--sync] [--port BUS-PORT[.PORT]]\n", argv[0]);
            return 2;
        }
    }
    libusb_context *context = NULL;
    int result = libusb_init(&context);
    if (result < 0) { fprintf(stderr, "libusb: %s\n", libusb_error_name(result)); return 1; }
    libusb_device **list = NULL, *chosen = NULL;
    ssize_t count = libusb_get_device_list(context, &list);
    if (count < 0) { libusb_exit(context); return 1; }
    struct libusb_device_descriptor descriptor = {0};
    unsigned matches = 0;
    char path[80];
    for (ssize_t i = 0; i < count; ++i) {
        struct libusb_device_descriptor current;
        if (libusb_get_device_descriptor(list[i], &current) < 0 ||
            current.idVendor != 0x5555 || current.idProduct != 0x1110) continue;
        physical_path(list[i], path, sizeof path);
        if (port && strcmp(port, path)) continue;
        printf("Original VGA2USB 5555:1110, port %s, device %u, bcdDevice %04x, USB speed %d\n",
               path, libusb_get_device_address(list[i]), current.bcdDevice, libusb_get_device_speed(list[i]));
        chosen = list[i];
        descriptor = current;
        matches++;
    }
    int status = 0;
    if (matches != 1) {
        fprintf(stderr, "Expected exactly one original VGA2USB%s; found %u\n", port ? " at the selected port" : "", matches);
        status = 1;
        goto done;
    }
    if (!strcmp(action, "--inspect")) goto done;
    if (!strcmp(action, "--sync") && descriptor.bcdDevice != firmware_version) {
        fprintf(stderr, "Sync query requires verified startup firmware %04x; bootstrap first\n", firmware_version);
        status = 1;
        goto done;
    }
    if (!strcmp(action, "--bootstrap") && descriptor.bcdDevice == firmware_version) {
        puts("Firmware descriptor already matches; no RAM upload performed");
        goto done;
    }
    libusb_device_handle *handle = NULL;
    result = libusb_open(chosen, &handle);
    if (result < 0) {
        fprintf(stderr, "Open: %s (USB read/write access is required)\n", libusb_error_name(result));
        status = 1;
        goto done;
    }
    result = libusb_claim_interface(handle, 0);
    if (result < 0) {
        fprintf(stderr, "Claim interface: %s\n", libusb_error_name(result));
        libusb_close(handle);
        status = 1;
        goto done;
    }
    if (!strcmp(action, "--bootstrap")) {
        printf("Loading original-model volatile RAM firmware, image SHA256 %s\n", firmware_hash);
        fflush(stdout);
        result = v2u_bootstrap(control_out, handle, firmware_segments,
                               sizeof firmware_segments / sizeof firmware_segments[0]);
        if (result < 0) {
            fprintf(stderr, "Firmware upload failed (%d); reconnect the VGA2USB before retrying\n", result);
            status = 1;
        } else puts("RAM upload and CPU release completed; verify the reenumerated descriptor before capture");
    } else {
        unsigned char bytes[10] = {0};
        result = libusb_control_transfer(handle, 0xc0, 0xb1, 0, 0, bytes, sizeof bytes, 0);
        if (result != sizeof bytes) {
            fprintf(stderr, "Sync status returned %d/10 bytes\n", result);
            status = 1;
        } else {
            printf("Sync status raw:");
            for (size_t i = 0; i < sizeof bytes; ++i) printf(" %02x", bytes[i]);
            printf("\n");
            unsigned counter = ((unsigned)bytes[0] << 8) | bytes[1];
            printf("First sync counter: %u%s\n", counter, counter == 0 ? " (vendor path reports no VGA signal)" : "");
        }
    }
    libusb_release_interface(handle, 0);
    libusb_close(handle);
done:
    libusb_free_device_list(list, 1);
    libusb_exit(context);
    return status;
}
