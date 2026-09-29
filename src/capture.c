#include <limits.h>
#include <stdlib.h>
#include "capture.h"

struct read_chunk {
    struct libusb_transfer *transfer;
    int pending, length;
    enum libusb_transfer_status status;
};
static void LIBUSB_CALL received(struct libusb_transfer *transfer)
{
    struct read_chunk *chunk = transfer->user_data;
    chunk->status = transfer->status;
    chunk->length = transfer->actual_length;
    chunk->pending = 0;
}

static int capture_wire(libusb_context *context, libusb_device_handle *handle,
                        const uint8_t command[31], uint8_t *wire,
                        const struct v2u_home_layout *layout)
{
    /* Vendor .text43140 bounds transfers to 128 KB; HOME rounds to whole rows. */
    size_t stride = layout->segment_stride;
    if (!stride || stride > 131072) return -1;
    size_t block = (131072 / stride) * stride;
    size_t count = (layout->wire_bytes + block - 1) / block;
    struct read_chunk *chunks = calloc(count, sizeof(*chunks));
    if (!chunks) return -1;
    int error = 0;
    size_t submitted = 0;
    for (size_t i = 0; i < count; ++i) {
        size_t offset = i * block;
        size_t length = layout->wire_bytes - offset;
        if (length > block) length = block;
        chunks[i].transfer = libusb_alloc_transfer(0);
        if (!chunks[i].transfer) { error = -1; break; }
        libusb_fill_bulk_transfer(chunks[i].transfer, handle, 0x82,
                                 wire + offset, (int)length, received, &chunks[i], 0);
    }
    if (!error) {
        for (size_t i = 0; i < count; ++i) {
            /* Control handling can deliver the first read's callback. */
            if (i && !chunks[0].pending &&
                (chunks[0].status != LIBUSB_TRANSFER_COMPLETED ||
                 chunks[0].length != chunks[0].transfer->length)) {
                error = chunks[0].status == LIBUSB_TRANSFER_COMPLETED ? -32 : -1; break;
            }
            error = libusb_submit_transfer(chunks[i].transfer);
            if (error < 0) break;
            chunks[i].pending = 1;
            ++submitted;
            if (i == 0) {
                /* First read before B4, remaining reads after B4: .text4584..460e. */
                int sent = libusb_control_transfer(handle, 0x40, 0xb4, 0, 0,
                                                   (unsigned char *)command, 31, 0);
                if (sent != 31) {
                    fprintf(stderr, "B4 setup returned %d/31 bytes\n", sent);
                    error = -1; break;
                }
            }
        }
    }
    int cancelling = 0;
    for (;;) {
        size_t pending = 0;
        for (size_t i = 0; i < submitted; ++i) {
            if (chunks[i].pending) ++pending;
            else if (chunks[i].status != LIBUSB_TRANSFER_COMPLETED ||
                     chunks[i].length != chunks[i].transfer->length) {
                if (!error) error = chunks[i].status == LIBUSB_TRANSFER_COMPLETED ? -32 : -1;
            }
        }
        if (!pending) break;
        if (error < 0 && !cancelling) {
            for (size_t i = 0; i < submitted; ++i)
                if (chunks[i].pending) libusb_cancel_transfer(chunks[i].transfer);
            cancelling = 1;
        }
        int event = libusb_handle_events_completed(context, NULL);
        if (event < 0 && event != LIBUSB_ERROR_INTERRUPTED) error = event;
    }
    size_t received_bytes = 0;
    for (size_t i = 0; i < count; ++i) {
        if (chunks[i].status == LIBUSB_TRANSFER_COMPLETED && chunks[i].length > 0)
            received_bytes += (size_t)chunks[i].length;
        if (chunks[i].transfer) libusb_free_transfer(chunks[i].transfer);
    }
    if (error < 0 || received_bytes != layout->wire_bytes) {
        fprintf(stderr, "Incomplete frame: %zu/%zu bytes in %zu submitted reads\n",
                received_bytes, layout->wire_bytes, submitted);
        if (!error) error = -32;
    }
    free(chunks);
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
    size_t frames = 0; unsigned retained_phases = 2, retained_width = 0, retained_height = 0;
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
        if (layout.width == retained_width && layout.height == retained_height) {
            while (layout.vertical_phases < retained_phases)
                if (v2u_capture_next_phase(command, &layout) < 0) return -1;
        }
        uint8_t *wire = malloc(layout.wire_bytes), *pixels = malloc(layout.output_bytes);
        if (!wire || !pixels) { free(wire); free(pixels); return -1; }
        int error, failures = 0;
        for (;;) {
            error = capture_wire(context, handle, command, wire, &layout);
            if (error != -32) break;
            if (++failures < 3) continue;
            if (v2u_capture_next_phase(command, &layout) < 0) break;
            failures = 0;
            fprintf(stderr, "HOME recovery: %u vertical phases\n", layout.vertical_phases);
        }
        if (!error) {
            retained_phases = layout.vertical_phases;
            retained_width = layout.width; retained_height = layout.height;
        }
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
