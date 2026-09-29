#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "home_frame.h"

static void check_order(unsigned width, unsigned height, unsigned v, unsigned p)
{
    struct v2u_home_layout layout;
    assert(v2u_home_layout(width, height, v, p, &layout) == 0);
    uint8_t *wire = malloc(layout.wire_bytes);
    uint8_t *out = malloc(layout.output_bytes + 2);
    assert(wire && out);
    memset(wire, 0xa5, layout.wire_bytes);
    memset(out, 0xcc, layout.output_bytes + 2);
    unsigned i = 0;
    /* Produce known pixels in wire phase order, independently of the decoder. */
    for (unsigned xphase = 0; xphase < p; ++xphase) {
        for (unsigned yphase = 0; yphase < v; ++yphase) {
            for (unsigned y = yphase; y < height; y += v, ++i) {
                size_t offset = i * layout.segment_stride;
                for (unsigned x = xphase, j = 0; x < width; x += p, ++j) {
                    uint16_t pixel = (uint16_t)(y * width + x);
                    wire[offset + 2*j] = (uint8_t)pixel;
                    wire[offset + 2*j + 1] = (uint8_t)(pixel >> 8);
                }
            }
        }
    }
    assert(v2u_home_decode(&layout, wire, layout.wire_bytes, out + 1, layout.output_bytes) == 0);
    assert(out[0] == 0xcc && out[layout.output_bytes+1] == 0xcc);
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            size_t offset = 1 + 2 * (y * width + x);
            uint16_t actual = (uint16_t)(out[offset] | (out[offset+1] << 8));
            assert(actual == (uint16_t)(y * width + x));
        }
    }
    memset(out, 0xcc, layout.output_bytes + 2);
    assert(v2u_home_decode(&layout, wire, layout.wire_bytes - 1, out + 1, layout.output_bytes) < 0);
    assert(v2u_home_decode(&layout, wire, layout.wire_bytes, out + 1, layout.output_bytes - 1) < 0);
    for (size_t j = 0; j < layout.output_bytes + 2; ++j) assert(out[j] == 0xcc);
    free(wire);
    free(out);
}

int main(void)
{
    struct v2u_home_layout layout;
    assert(v2u_home_layout(640, 480, 2, 1, &layout) == 0);
    assert(layout.wire_bytes == 737024 && layout.output_bytes == 614400);
    assert(layout.segment_stride == 1536 && layout.useful_bytes == 1280);
    assert(v2u_home_layout(720, 400, 2, 1, &layout) == 0);
    assert(layout.wire_bytes == 614304 && layout.output_bytes == 576000);
    assert(v2u_home_layout(640, 480, 0, 1, &layout) < 0);
    assert(v2u_home_layout(640, 480, 2, 0, &layout) < 0);
    assert(v2u_home_layout(641, 480, 2, 2, &layout) < 0);
    assert(v2u_home_layout(640, 479, 2, 1, &layout) < 0);
    assert(v2u_home_layout(0, 400, 2, 1, &layout) < 0);
    check_order(6, 4, 2, 1);
    check_order(8, 6, 3, 2);
    check_order(720, 400, 2, 1);
    check_order(640, 480, 2, 1);
    puts("HOME frame: VGA byte counts, row/sample deordering, padding and bounds passed");
    return 0;
}
