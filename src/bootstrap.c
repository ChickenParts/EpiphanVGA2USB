#include "bootstrap.h"

int v2u_bootstrap(v2u_control_out write, void *context,
                  const struct v2u_segment *segments, size_t count)
{
    if (!write || !segments || count == 0 || count > 256) return -1;
    for (size_t i = 0; i < count; ++i) {
        if (!segments[i].data || segments[i].length == 0 ||
            segments[i].address >= 0x2000 ||
            segments[i].length > 0x2000 - segments[i].address) return -1;
    }
    const uint8_t halt = 1, run = 0;
    int result = write(context, 0xa0, 0xe600, 0, &halt, 1);
    if (result != 1) return result < 0 ? result : -1;
    for (size_t i = 0; i < count; ++i) {
        for (size_t offset = 0; offset < segments[i].length; offset += 64) {
            size_t remaining = segments[i].length - offset;
            uint16_t length = (uint16_t)(remaining > 64 ? 64 : remaining);
            result = write(context, 0xa0, (uint16_t)(segments[i].address + offset),
                           0, segments[i].data + offset, length);
            if (result != length) return result < 0 ? result : -1;
        }
    }
    result = write(context, 0xa0, 0xe600, 0, &run, 1);
    return result == 1 ? 0 : (result < 0 ? result : -1);
}
