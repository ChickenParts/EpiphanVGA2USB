#include <limits.h>
#include <stdlib.h>
#include "capture.h"

struct read_result { int done, length; enum libusb_transfer_status status; };
static void LIBUSB_CALL received(struct libusb_transfer *transfer)
{
    struct read_result *result = transfer->user_data;
    result->status = transfer->status;
    result->length = transfer->actual_length;
    result->done = 1;
}

static int capture_wire(libusb_context *context, libusb_device_handle *handle,
                        const uint8_t command[31], uint8_t *wire, size_t length)
{
    if (length > INT_MAX) return -1;
    struct libusb_transfer *transfer = libusb_alloc_transfer(0);
    if (!transfer) return -1;
    struct read_result result = {0};
    libusb_fill_bulk_transfer(transfer, handle, 0x82, wire, (int)length,
                              received, &result, 0);
    int error = libusb_submit_transfer(transfer);
    if (error < 0) { libusb_free_transfer(transfer); return error; }
    /* The vendor queues a read before issuing B4. Preserve that ordering. */
    int sent = libusb_control_transfer(handle, 0x40, 0xb4, 0, 0,
                                       (unsigned char *)command, 31, 0);
    if (sent != 31) {
        fprintf(stderr, "B4 setup returned %d/31 bytes\n", sent);
        libusb_cancel_transfer(transfer);
        error = -1;
    }
    while (!result.done) {
        int event = libusb_handle_events_completed(context, &result.done);
        if (event < 0 && event != LIBUSB_ERROR_INTERRUPTED) {
            error = event;
            libusb_cancel_transfer(transfer);
        }
    }
    if (result.status != LIBUSB_TRANSFER_COMPLETED || result.length != (int)length) {
        fprintf(stderr, "Frame transport status %d, %d/%zu bytes\n",
                result.status, result.length, length);
        error = -1;
    }
    libusb_free_transfer(transfer);
    return error < 0 ? error : 0;
}

static int ppm(FILE *output, const struct v2u_home_layout *layout, const uint8_t *pixels)
{
    if (fprintf(output, "P6\n%u %u\n255\n", layout->width, layout->height) < 0) return -1;
    uint8_t *row = malloc(3 * (size_t)layout->width);
    if (!row) return -1;
    int error = 0;
    for (unsigned y = 0; y < layout->height; ++y) {
        for (unsigned x = 0; x < layout->width; ++x) {
            size_t offset = 2 * ((size_t)y * layout->width + x);
            unsigned value = pixels[offset] | ((unsigned)pixels[offset+1] << 8);
            unsigned r = (value >> 11) & 31, g = (value >> 5) & 63, b = value & 31;
            row[3*x] = (uint8_t)((r << 3) | (r >> 2));
            row[3*x+1] = (uint8_t)((g << 2) | (g >> 4));
            row[3*x+2] = (uint8_t)((b << 3) | (b >> 2));
        }
        if (fwrite(row, 3, layout->width, output) != layout->width) { error = -1; break; }
    }
    free(row);
    return fflush(output) == 0 ? error : -1;
}

int v2u_capture_ppm(libusb_context *context, libusb_device_handle *handle,
                     enum v2u_mode requested, FILE *output, int continuous)
{
    size_t frames = 0;
    do {
        uint8_t status[10], command[31];
        int got = libusb_control_transfer(handle, 0xc0, 0xb1, 0, 0, status, sizeof status, 0);
        if (got != sizeof status) { fprintf(stderr, "Mode status returned %d/10 bytes\n", got); return -1; }
        struct v2u_home_layout layout;
        if (v2u_capture_mode(status, requested, command, &layout) < 0) {
            fprintf(stderr, "Absent or unsupported VGA timing; auto supports 720x400@70.\n"
                            "Select --mode 640x480 explicitly for verified 640x480@59.94 sources.\n");
            return -1;
        }
        uint8_t *wire = malloc(layout.wire_bytes), *pixels = malloc(layout.output_bytes);
        if (!wire || !pixels) { free(wire); free(pixels); return -1; }
        int error = capture_wire(context, handle, command, wire, layout.wire_bytes);
        if (!error) error = v2u_home_decode(&layout, wire, layout.wire_bytes, pixels, layout.output_bytes);
        if (!error) error = ppm(output, &layout, pixels);
        free(wire);
        free(pixels);
        if (error < 0) return error;
        frames++;
        if (frames == 1) fprintf(stderr, "Captured %ux%u, %zu wire bytes, original RGB565 HOME format\n",
                                layout.width, layout.height, layout.wire_bytes);
    } while (continuous);
    return 0;
}
