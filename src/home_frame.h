#ifndef V2U_HOME_FRAME_H
#define V2U_HOME_FRAME_H
#include <stddef.h>
#include <stdint.h>

struct v2u_home_layout {
    unsigned width, height, vertical_phases, horizontal_phases;
    size_t useful_bytes, segment_stride, wire_bytes, output_bytes;
};
int v2u_home_layout(unsigned width, unsigned height, unsigned vertical_phases,
                     unsigned horizontal_phases, struct v2u_home_layout *layout);
int v2u_home_decode(const struct v2u_home_layout *layout,
                     const uint8_t *wire, size_t wire_size,
                     uint8_t *output, size_t output_size);
#endif
