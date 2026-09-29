#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "capture_mode.h"
int main(void)
{
    /* First actual live-signal observation, preserved unmodified. */
    const uint8_t measured720[10] = {0x6f,0x7d,0x01,0xc2,0,0xf2,0x0a,0xc6,2,0};
    const uint8_t expected720[31] = {0x03,0x84,2,0,8,0x14,0x1f,0x70,0x70,0x70,0x20,0x20,0x20,3,
      2,0xd0,1,0x90,0,2,0,0xc8,0,0x9e,0x24,2,1,0x80,0x80,0,0};
    uint8_t command[31];
    struct v2u_home_layout layout;
    assert(v2u_capture_mode(measured720, V2U_MODE_AUTO, command, &layout) == 0);
    assert(memcmp(command, expected720, sizeof command) == 0);
    assert(layout.width == 720 && layout.height == 400);
    assert(v2u_capture_mode(measured720, V2U_MODE_640_480, command, &layout) < 0);
    const uint8_t synthetic640[10] = {0x82,0x35,2,0x0d,0,0,0,0,0xff,0};
    assert(v2u_capture_mode(synthetic640, V2U_MODE_AUTO, command, &layout) < 0);
    assert(v2u_capture_mode(synthetic640, V2U_MODE_640_480, command, &layout) == 0);
    const uint8_t expected640[31] = {3,0x20,3,0,8,0x14,0x1f,0x70,0x70,0x70,0x20,0x20,0x20,3,
      2,0x80,1,0xe0,0,2,0,0xf0,0,0x90,0x21,2,1,0x80,0x80,0,0};
    assert(memcmp(command, expected640, sizeof command) == 0);
    uint8_t invalid[10] = {0};
    memset(command, 0xcc, sizeof command);
    assert(v2u_capture_mode(invalid, V2U_MODE_AUTO, command, &layout) < 0);
    for (size_t i=0; i<sizeof command; ++i) assert(command[i] == 0xcc);
    memcpy(invalid, measured720, sizeof invalid);
    invalid[3] = 0xc3; /* Different total vertical line count. */
    assert(v2u_capture_mode(invalid, V2U_MODE_AUTO, command, &layout) < 0);
    puts("Capture mode: original ADC overrides, real text-mode status, geometry rejection passed");
    return 0;
}
