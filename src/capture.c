#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "capture.h"
#include "source_watch.h"
#include "stream_frame.h"

#define V2U_FRAME_RECHECK (-34)

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

static int source_matches(libusb_device_handle *handle, const uint8_t command[31],
                          const struct v2u_home_layout *layout, enum v2u_mode requested)
{
    uint8_t status[10], measured_command[31];
    struct v2u_home_layout measured;
    int got=libusb_control_transfer(handle,0xc0,0xb1,0,0,status,sizeof status,0);

    if (got < 0) return got;
    if (got != sizeof status) return V2U_CAPTURE_REOPEN;
    if (v2u_capture_mode(status,requested,measured_command,&measured) < 0 ||
        memcmp(measured_command,command,18) ||
        memcmp(measured_command+22,command+22,3) ||
        measured.horizontal_phases != layout->horizontal_phases) return V2U_CAPTURE_REOPEN;
    return 0;
}

static int capture_wire(libusb_context *context, libusb_device_handle *handle,
                        const uint8_t command[31], uint8_t *wire,
                        const struct v2u_home_layout *layout, unsigned period)
{
    /* Vendor .text43140 bounds transfers to 128 KB; HOME rounds to whole rows. */
    size_t stride = layout->segment_stride;
    if (!stride || stride > 131072) return -1;
    size_t block = (131072 / stride) * stride;
    size_t count = (layout->wire_bytes + block - 1) / block;
    struct read_chunk *chunks = calloc(count, sizeof(*chunks));
    if (!chunks) return -1;
    struct v2u_source_watch watch;
    /* Allow four complete phase schedules, at least one second between checks. */
    unsigned cadence_ms = (unsigned)((uint64_t)period * layout->vertical_phases * layout->horizontal_phases * 4 / 2000);
    if (cadence_ms < 1000) cadence_ms=1000;
    int error = v2u_source_watch_init(&watch, context, cadence_ms);
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
        int probe = 0;
        if (v2u_source_watch_wait(&watch, &probe) < 0) error = -1;
        /* B1 aborts GPIF in original firmware: measure only outside image reads. */
        /* poll already waited for readiness; handle available callbacks without waiting. */
        struct timeval ready = {0,0};
        int event = libusb_handle_events_timeout_completed(context, &ready, NULL);
        if (event < 0 && event != LIBUSB_ERROR_INTERRUPTED) error = event;
        if (probe && !error) {
            for (size_t i=0; i<submitted; ++i)
                if (chunks[i].pending) { error=V2U_FRAME_RECHECK; break; }
        }
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
    v2u_source_watch_destroy(&watch);
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

static int clear_display(FILE *output, const struct v2u_home_layout *layout)
{
    uint8_t *pixels=calloc(1,layout->output_bytes);
    if (!pixels) return -1;
    int result=v2u_stream_ppm(output,layout,pixels);
    free(pixels); return result;
}
static void wait_for_signal(void)
{
    struct timespec wait={0,100000000};
    while (nanosleep(&wait,&wait) < 0 && errno == EINTR) {}
}

int v2u_capture_ppm(libusb_context *context, libusb_device_handle *handle,
                     enum v2u_mode requested, FILE *output, int continuous)
{
    struct v2u_home_layout display;
    if (v2u_home_layout(requested == V2U_MODE_640_480 ? 640 : 720,
                        requested == V2U_MODE_640_480 ? 480 : 400,2,1,&display) < 0) return -1;
    int waiting = 0, observed = 0;
    size_t frames = 0; unsigned retained_phases = 2, retained_width = 0, retained_height = 0;
    do {
        uint8_t status[10], command[31];
        int got = libusb_control_transfer(handle, 0xc0, 0xb1, 0, 0, status, sizeof status, 0);
        if (got != sizeof status && (got < 0 || !continuous)) { fprintf(stderr, "Mode status returned %d/10 bytes\n", got); return -1; }
        struct v2u_home_layout layout;
        if (got != sizeof status || v2u_capture_mode(status, requested, command, &layout) < 0) {
            if (!continuous) {
                fprintf(stderr, "Absent or unsupported progressive VGA timing (up to 1024x768).\n"
                                "Use an explicit --mode preset when vertical timing is ambiguous.\n");
                return -1;
            }
            if (!waiting) {
                fprintf(stderr, "Waiting for supported VGA signal; viewer remains open\n");
                if (clear_display(output,&display) < 0) return -1;
            }
            if (observed) return V2U_CAPTURE_REOPEN;
            waiting=1; retained_width=retained_height=0;
            wait_for_signal();
            continue;
        }
        display=layout; observed=1;
        if (waiting) fprintf(stderr, "VGA signal returned; resuming capture\n");
        waiting=0;
        if (layout.width == retained_width && layout.height == retained_height) {
            while (layout.vertical_phases < retained_phases)
                if (v2u_capture_next_phase(command, &layout) < 0) return -1;
        }
        uint8_t *wire = malloc(layout.wire_bytes), *pixels = malloc(layout.output_bytes);
        if (!wire || !pixels) { free(wire); free(pixels); return -1; }
        int error, failures = 0, reacquisitions = 0;
        for (;;) {
            error = capture_wire(context, handle, command, wire, &layout,
                                 ((unsigned)status[0] << 8) | status[1]);
            if (error == V2U_FRAME_RECHECK)
                fprintf(stderr, "Image reads drained for scheduled VGA timing reacquisition\n");
            int scheduled = error == V2U_FRAME_RECHECK;
            if (!scheduled && error != -32) break;
            int signal=source_matches(handle,command,&layout,requested);
            if (signal < 0) { error=signal; break; }
            if (scheduled) {
                if (++reacquisitions < 3) continue;
                fprintf(stderr, "Repeated unfinished capture; reopening with unchanged VGA timing\n");
                error=continuous ? V2U_CAPTURE_REOPEN : -32;
                break;
            }
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
        if (!error) error = continuous ? v2u_stream_ppm(output, &layout, pixels) : ppm(output, &layout, pixels);
        free(wire);
        free(pixels);
        if (error == V2U_CAPTURE_REOPEN && continuous) {
            fprintf(stderr,"VGA status unavailable or changed during frame; waiting for supported signal\n");
            if (clear_display(output,&display) < 0) return -1;
            return V2U_CAPTURE_REOPEN;
        }
        if (error < 0) return error;
        frames++;
        if (continuous && frames % 64 == 0) {
            struct timespec now;
            if (clock_gettime(CLOCK_MONOTONIC,&now) == 0)
                fprintf(stderr,"Frame %zu delivered at %lld.%03ld, V=%u\n",frames,
                        (long long)now.tv_sec,now.tv_nsec/1000000,layout.vertical_phases);
        }
        if (frames == 1) fprintf(stderr, "Captured %ux%u, %zu wire bytes, original RGB565 HOME format\n",
                                layout.width, layout.height, layout.wire_bytes);
    } while (continuous);
    return 0;
}
