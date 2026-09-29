#include "stream_frame.h"
#include <stdlib.h>

int v2u_stream_ppm(FILE *output, const struct v2u_home_layout *layout,
                   const uint8_t *pixels)
{
    struct v2u_home_layout checked;
    if (!output || !layout || !pixels ||
        v2u_home_layout(layout->width, layout->height, layout->vertical_phases,
                         layout->horizontal_phases, &checked) < 0 ||
        layout->output_bytes != checked.output_bytes) return -1;

    static const char header[] = "P6\n720 480\n255\n";
    uint8_t *row = malloc(V2U_STREAM_WIDTH * 3);
    if (!row) return -1;
    int result = -1;
    if (fwrite(header, 1, sizeof(header) - 1, output) != sizeof(header) - 1)
        goto done;
    for (unsigned y = 0; y < V2U_STREAM_HEIGHT; ++y) {
        unsigned source_y = y * checked.height / V2U_STREAM_HEIGHT;
        for (unsigned x = 0; x < V2U_STREAM_WIDTH; ++x) {
            unsigned source_x = x * checked.width / V2U_STREAM_WIDTH;
            size_t offset = 2 * ((size_t)source_y * checked.width + source_x);
            uint16_t pixel = (uint16_t)(pixels[offset] | (pixels[offset + 1] << 8));
            unsigned red = (pixel >> 11) & 31;
            unsigned green = (pixel >> 5) & 63;
            unsigned blue = pixel & 31;
            row[3 * x] = (uint8_t)((red << 3) | (red >> 2));
            row[3 * x + 1] = (uint8_t)((green << 2) | (green >> 4));
            row[3 * x + 2] = (uint8_t)((blue << 3) | (blue >> 2));
        }
        if (fwrite(row, 1, V2U_STREAM_WIDTH * 3, output) != V2U_STREAM_WIDTH * 3)
            goto done;
    }
    if (fflush(output) == 0) result = 0;
done:
    free(row);
    return result;
}
