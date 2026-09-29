#include <string.h>
#include "capture_mode.h"

int v2u_capture_mode(const uint8_t status[10], enum v2u_mode requested,
                     uint8_t command[31], struct v2u_home_layout *layout)
{
    if (!status || !command || !layout) return -1;
    unsigned period = ((unsigned)status[0] << 8) | status[1];
    unsigned lines = ((unsigned)status[2] << 8) | status[3];
    if (!period) return -1;
    unsigned refresh = 2000000000u / period;
    enum v2u_mode selected = requested;
    if (selected == V2U_MODE_AUTO && lines == 450 && refresh >= 69000 && refresh <= 71000)
        selected = V2U_MODE_720_400;
    if (selected == V2U_MODE_720_400 && lines == 450 && refresh >= 69000 && refresh <= 71000) {
        const uint8_t preset[31] = {3,0x84,0,0,8,0x14,0x1f,0x70,0x70,0x70,0x20,0x20,0x20,3,
          2,0xd0,1,0x90,0,2,0,0xc8,0,0x9e,0x24,2,1,0x80,0x80,0,0};
        if (v2u_home_layout(720, 400, 2, 1, layout) < 0) return -1;
        memcpy(command, preset, sizeof preset);
    } else if (selected == V2U_MODE_640_480 && lines == 525 && refresh >= 58940 && refresh <= 60940) {
        const uint8_t preset[31] = {3,0x20,0,0,8,0x14,0x1f,0x70,0x70,0x70,0x20,0x20,0x20,3,
          2,0x80,1,0xe0,0,2,0,0xf0,0,0x90,0x21,2,1,0x80,0x80,0,0};
        if (v2u_home_layout(640, 480, 2, 1, layout) < 0) return -1;
        memcpy(command, preset, sizeof preset);
    } else return -1;
    command[2] = status[8] & 3;
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
