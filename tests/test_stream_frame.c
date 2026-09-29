#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stream_frame.h"

static const char header[] = "P6\n720 480\n255\n";

static uint16_t pattern(unsigned x, unsigned y, unsigned width, unsigned height)
{
    if (y < 3) return x < 3 ? 0xffff : 0xf800;
    if (y >= height - 3) return x >= width - 3 ? 0x001f : 0x07e0;
    if (x < 3) return 0x07ff;
    if (x >= width - 3) return 0xf81f;
    return (uint16_t)(((x % 32) << 11) | ((y % 64) << 5) | ((x + y) % 32));
}

static void check_resize(unsigned width, unsigned height)
{
    struct v2u_home_layout layout;
    assert(v2u_home_layout(width, height, 1, 1, &layout) == 0);
    uint8_t *input = malloc(layout.output_bytes);
    uint8_t *copy = malloc(layout.output_bytes);
    uint8_t *frame = malloc(V2U_STREAM_FRAME_BYTES);
    FILE *output = tmpfile();
    assert(input && copy && frame && output);
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            uint16_t color = pattern(x, y, width, height);
            size_t offset = 2 * ((size_t)y * width + x);
            input[offset] = (uint8_t)color;
            input[offset + 1] = (uint8_t)(color >> 8);
        }
    }
    memcpy(copy, input, layout.output_bytes);
    assert(v2u_stream_ppm(output, &layout, input) == 0);
    assert(ftell(output) == V2U_STREAM_FRAME_BYTES);
    rewind(output);
    assert(fread(frame, 1, V2U_STREAM_FRAME_BYTES, output) == V2U_STREAM_FRAME_BYTES);
    assert(fgetc(output) == EOF);
    assert(memcmp(frame, header, sizeof(header) - 1) == 0);
    for (unsigned y = 0; y < V2U_STREAM_HEIGHT; ++y) {
        for (unsigned x = 0; x < V2U_STREAM_WIDTH; ++x) {
            uint16_t color = pattern(x * width / V2U_STREAM_WIDTH,
                                     y * height / V2U_STREAM_HEIGHT, width, height);
            unsigned r = (color >> 11) & 31;
            unsigned g = (color >> 5) & 63;
            unsigned b = color & 31;
            size_t offset = V2U_STREAM_HEADER_BYTES +
                3 * ((size_t)y * V2U_STREAM_WIDTH + x);
            assert(frame[offset] == ((r << 3) | (r >> 2)));
            assert(frame[offset + 1] == ((g << 2) | (g >> 4)));
            assert(frame[offset + 2] == ((b << 3) | (b >> 2)));
        }
    }
    assert(memcmp(input, copy, layout.output_bytes) == 0);
    fclose(output);
    free(input);
    free(copy);
    free(frame);
}

static void check_rejection(void)
{
    struct v2u_home_layout layout;
    uint8_t input[2] = {0, 0};
    FILE *output = tmpfile();
    assert(output);
    assert(v2u_home_layout(1, 1, 1, 1, &layout) == 0);
    assert(v2u_stream_ppm(NULL, &layout, input) < 0);
    assert(v2u_stream_ppm(output, NULL, input) < 0);
    assert(v2u_stream_ppm(output, &layout, NULL) < 0);
    layout.width = 0;
    assert(v2u_stream_ppm(output, &layout, input) < 0);
    layout.width = 4097;
    assert(v2u_stream_ppm(output, &layout, input) < 0);
    layout.width = 1;
    layout.height = 0;
    assert(v2u_stream_ppm(output, &layout, input) < 0);
    layout.height = 4097;
    assert(v2u_stream_ppm(output, &layout, input) < 0);
    layout.height = 1;
    layout.output_bytes = 1;
    assert(v2u_stream_ppm(output, &layout, input) < 0);
    layout.output_bytes = 3;
    assert(v2u_stream_ppm(output, &layout, input) < 0);
    assert(ftell(output) == 0);
    fclose(output);
    assert(v2u_home_layout(1, 1, 1, 1, &layout) == 0);
    output = fopen("/dev/full", "wb");
    assert(output);
    assert(v2u_stream_ppm(output, &layout, input) < 0);
    fclose(output);
    output = fopen("/dev/full", "wb");
    assert(output);
    char *buffer = malloc(V2U_STREAM_FRAME_BYTES * 2);
    assert(buffer);
    assert(setvbuf(output, buffer, _IOFBF, V2U_STREAM_FRAME_BYTES * 2) == 0);
    assert(v2u_stream_ppm(output, &layout, input) < 0);
    fclose(output);
    free(buffer);
}

static void sequence(void)
{
    static const unsigned widths[] = {720, 640, 800, 1024};
    static const unsigned heights[] = {400, 480, 600, 768};
    static const unsigned counts[] = {200, 1, 199, 200};
    static const uint16_t colors[] = {0xf800, 0, 0x07e0, 0x07e0};
    for (size_t i = 0; i < sizeof(widths) / sizeof(widths[0]); ++i) {
        struct v2u_home_layout layout;
        assert(v2u_home_layout(widths[i], heights[i], 1, 1, &layout) == 0);
        uint8_t *pixels = malloc(layout.output_bytes);
        assert(pixels);
        for (size_t j = 0; j < layout.output_bytes; j += 2) {
            pixels[j] = (uint8_t)colors[i];
            pixels[j + 1] = (uint8_t)(colors[i] >> 8);
        }
        for (unsigned j = 0; j < counts[i]; ++j)
            assert(v2u_stream_ppm(stdout, &layout, pixels) == 0);
        free(pixels);
    }
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--sequence") == 0) {
        sequence();
        return 0;
    }
    assert(sizeof(header) - 1 == V2U_STREAM_HEADER_BYTES);
    assert(V2U_STREAM_FRAME_BYTES == 1036815);
    check_resize(1, 1);
    check_resize(640, 480);
    check_resize(720, 400);
    check_resize(800, 600);
    check_resize(1024, 768);
    check_resize(1280, 1024);
    check_rejection();
    puts("Stream frame: fixed P6 canvas, RGB565 resizing, input immutability and output errors passed");
    return 0;
}
