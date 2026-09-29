#include "home_frame.h"

int v2u_home_layout(unsigned width, unsigned height, unsigned v, unsigned p,
                     struct v2u_home_layout *layout)
{
    if (!layout || width == 0 || height == 0 || width > 4096 || height > 4096 ||
        v == 0 || p == 0 || height % v || width % p) return -1;
    size_t useful = 2 * (size_t)(width / p);
    size_t stride = (useful + 511) & ~(size_t)511;
    size_t segments = (size_t)height * p;
    *layout = (struct v2u_home_layout){width, height, v, p, useful, stride,
        (segments - 1) * stride + useful, 2 * (size_t)width * height};
    return 0;
}

int v2u_home_decode(const struct v2u_home_layout *layout,
                     const uint8_t *wire, size_t wire_size,
                     uint8_t *output, size_t output_size)
{
    if (!layout || !wire || !output) return -1;
    struct v2u_home_layout checked;
    if (v2u_home_layout(layout->width, layout->height, layout->vertical_phases,
                         layout->horizontal_phases, &checked) < 0 ||
        wire_size != checked.wire_bytes || output_size < checked.output_bytes) return -1;
    unsigned lines = checked.height / checked.vertical_phases;
    size_t segments = (size_t)checked.height * checked.horizontal_phases;
    for (size_t i = 0; i < segments; ++i) {
        unsigned xphase = (unsigned)(i / checked.height);
        unsigned r = (unsigned)(i % checked.height);
        unsigned y = (r % lines) * checked.vertical_phases + r / lines;
        const uint8_t *row = wire + i * checked.segment_stride;
        for (unsigned j = 0; j < checked.width / checked.horizontal_phases; ++j) {
            unsigned x = xphase + j * checked.horizontal_phases;
            size_t destination = 2 * ((size_t)y * checked.width + x);
            output[destination] = row[2*j];
            output[destination+1] = row[2*j+1];
        }
    }
    return 0;
}
