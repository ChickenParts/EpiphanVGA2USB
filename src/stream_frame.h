#ifndef V2U_STREAM_FRAME_H
#define V2U_STREAM_FRAME_H
#include <stdio.h>
#include "home_frame.h"

enum {
    V2U_STREAM_WIDTH = 720,
    V2U_STREAM_HEIGHT = 480,
    V2U_STREAM_HEADER_BYTES = 15,
    V2U_STREAM_FRAME_BYTES = V2U_STREAM_HEADER_BYTES +
        V2U_STREAM_WIDTH * V2U_STREAM_HEIGHT * 3
};

/* Write one fixed-size P6 canvas from decoded little-endian RGB565 pixels. */
int v2u_stream_ppm(FILE *output, const struct v2u_home_layout *layout,
                   const uint8_t *pixels);
#endif
