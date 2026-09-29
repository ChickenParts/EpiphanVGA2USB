#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bootstrap.h"

struct fake_usb {
    unsigned calls, fail_call, releases;
    uint16_t addresses[8], lengths[8];
    uint8_t bytes[8][64];
};

static int record(void *context, uint8_t request, uint16_t value, uint16_t index,
                  const uint8_t *data, uint16_t length)
{
    struct fake_usb *usb = context;
    assert(request == 0xa0 && index == 0 && length <= 64);
    assert(usb->calls < 8);
    unsigned n = usb->calls++;
    usb->addresses[n] = value;
    usb->lengths[n] = length;
    memcpy(usb->bytes[n], data, length);
    if (value == 0xe600 && length == 1 && data[0] == 0) usb->releases++;
    if (usb->calls == usb->fail_call) return length - 1;
    return length;
}

int main(void)
{
    uint8_t data[67];
    for (unsigned i = 0; i < sizeof data; ++i) data[i] = (uint8_t)i;
    const struct v2u_segment segments[] = {{0x100, sizeof data, data}, {0, 3, data}};
    struct fake_usb usb = {0};
    assert(v2u_bootstrap(record, &usb, segments, 2) == 0);
    assert(usb.calls == 5 && usb.releases == 1);
    assert(usb.addresses[0] == 0xe600 && usb.bytes[0][0] == 1);
    assert(usb.addresses[1] == 0x100 && usb.lengths[1] == 64);
    assert(usb.addresses[2] == 0x140 && usb.lengths[2] == 3);
    assert(memcmp(usb.bytes[2], data + 64, 3) == 0);
    assert(usb.addresses[3] == 0 && usb.lengths[3] == 3);
    assert(usb.addresses[4] == 0xe600 && usb.bytes[4][0] == 0);

    /* Every incomplete data transfer stops before MCU release. */
    for (unsigned fail = 1; fail <= 4; ++fail) {
        usb = (struct fake_usb){.fail_call = fail};
        assert(v2u_bootstrap(record, &usb, segments, 2) < 0);
        assert(usb.calls == fail && usb.releases == 0);
    }

    /* Validate the entire image before any write, including later segments. */
    const struct v2u_segment invalid[] = {{0x100, 3, data}, {0x1ffe, 3, data}};
    usb = (struct fake_usb){0};
    assert(v2u_bootstrap(record, &usb, invalid, 2) < 0 && usb.calls == 0);
    assert(v2u_bootstrap(record, &usb, segments, 0) < 0 && usb.calls == 0);
    assert(v2u_bootstrap(NULL, &usb, segments, 2) < 0 && usb.calls == 0);
    puts("Bootstrap: exact chunk/order, short-transfer stop, full-image validation passed");
    return 0;
}
