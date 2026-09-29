#include <string.h>
#include "capture_mode.h"
#include "vesa_modes.h"

static void store_be16(uint8_t *destination, unsigned value)
{
    destination[0] = (uint8_t)(value >> 8);
    destination[1] = (uint8_t)value;
}

int v2u_capture_mode(const uint8_t status[10], enum v2u_mode requested,
                     uint8_t command[31], struct v2u_home_layout *layout)
{
    if (!status || !command || !layout || requested < V2U_MODE_AUTO ||
        requested > V2U_MODE_720_400) return -1;
    unsigned period = ((unsigned)status[0] << 8) | status[1];
    unsigned lines = ((unsigned)status[2] << 8) | status[3];
    if (!period) return -1;
    unsigned refresh_tenths = 20000000u / period;
    const struct v2u_vesa_timing *selected = NULL;
    int64_t best_score = 3000;
    for (size_t i = 0; i < sizeof v2u_vesa_modes / sizeof v2u_vesa_modes[0]; ++i) {
        const struct v2u_vesa_timing *mode = &v2u_vesa_modes[i];
        /* Explicit presets pin the proven timing record, including refresh. */
        if (requested == V2U_MODE_640_480 &&
            (mode->width != 640 || mode->height != 480 || mode->refresh != 59940)) continue;
        if (requested == V2U_MODE_720_400 &&
            (mode->width != 720 || mode->height != 400 || mode->refresh != 70000)) continue;
        if (lines < mode->height + 6) continue;
        unsigned total_v = mode->height + mode->vfront + mode->vsync + mode->vback;
        int64_t dr = (int64_t)((mode->refresh + 49) / 100) - refresh_tenths;
        int64_t dl = (int64_t)total_v - lines;
        int64_t score = dr * dr + dl * dl;
        if (score < best_score) {
            selected = mode;
            best_score = score;
        }
    }
    if (!selected) return -1;
    unsigned total_h = selected->width + selected->hfront + selected->hsync + selected->hback;
    unsigned total_v = selected->height + selected->vfront + selected->vsync + selected->vback;
    /* AD9884A_hinterlaces .text+0x113d0: divide before multiplying. */
    uint64_t pixel = ((uint64_t)total_h * total_v / 1000) * selected->refresh;
    unsigned q_width = (selected->width + 1023) / 1024;
    unsigned q_pixel = (unsigned)((pixel + 49999999) / 50000000);
    unsigned q = q_width > q_pixel ? q_width : q_pixel;
    unsigned partitions = q <= 1 ? 1 : ((q + 1) & ~1u);
    struct v2u_home_layout generated;
    if (v2u_home_layout(selected->width, selected->height, 2, partitions, &generated) < 0)
        return -1;
    /* Fresh original AD9884A analog defaults and HOME progressive V=2. */
    uint8_t parameters[31] = {0,0,0,0,8,20,31,112,112,112,32,32,32,3,
        0,0,0,0,0,2,0,0,0,0,0,2,0,128,128,0,0};
    store_be16(parameters, total_h);
    parameters[2] = status[8] & 3;
    if (partitions == 1)
        parameters[3] = pixel < 35000000 ? 0 : (pixel < 50000000 ? 4 : 8);
    else
        parameters[3] = pixel < 50000000 ? 0 : (pixel < 100000000 ? 4 : 0x24);
    /* Every retained width/partition is below the 1366-pixel clamp threshold. */
    int clamp_delta = (int)selected->hback - 16;
    parameters[5] = (uint8_t)(clamp_delta < 8 ? 8 : (clamp_delta > 20 ? 20 : clamp_delta));
    store_be16(parameters + 14, selected->width);
    store_be16(parameters + 16, selected->height);
    store_be16(parameters + 20, selected->height / 2);
    store_be16(parameters + 22, selected->hsync + selected->hback);
    parameters[24] = (uint8_t)(selected->vback + ((selected->flags & 0x80) ? selected->vsync : 0));
    parameters[26] = (uint8_t)partitions;
    memcpy(command, parameters, sizeof parameters);
    *layout = generated;
    return 0;
}

int v2u_capture_next_phase(uint8_t command[31], struct v2u_home_layout *layout)
{
    if (!command || !layout) return -1;
    unsigned next = layout->vertical_phases + 1;
    while (next < 255 && layout->height % next) ++next;
    if (next >= 255) return -1;
    struct v2u_home_layout updated;
    if (v2u_home_layout(layout->width, layout->height, next,
                        layout->horizontal_phases, &updated) < 0) return -1;
    unsigned rows = layout->height / next;
    command[18] = (uint8_t)(next >> 8);
    command[19] = (uint8_t)next;
    command[20] = (uint8_t)(rows >> 8);
    command[21] = (uint8_t)rows;
    command[25] = (uint8_t)next;
    *layout = updated;
    return 0;
}
