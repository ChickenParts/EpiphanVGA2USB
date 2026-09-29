#ifndef V2U_BOOTSTRAP_H
#define V2U_BOOTSTRAP_H
#include <stddef.h>
#include <stdint.h>

struct v2u_segment {
    uint32_t address;
    size_t length;
    const uint8_t *data;
};

/* Return transferred byte count, or a negative error. */
typedef int (*v2u_control_out)(void *context, uint8_t request, uint16_t value,
                            uint16_t index, const uint8_t *data, uint16_t length);

/* A failure after CPU halt deliberately leaves it halted. Reconnect to recover. */
int v2u_bootstrap(v2u_control_out write, void *context,
                  const struct v2u_segment *segments, size_t count);
#endif
