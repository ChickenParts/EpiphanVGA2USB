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
    assert(v2u_capture_mode(synthetic640, V2U_MODE_AUTO, command, &layout) == 0);
    assert(layout.width == 640 && layout.height == 480);
    assert(v2u_capture_mode(synthetic640, V2U_MODE_640_480, command, &layout) == 0);
    const uint8_t expected640[31] = {3,0x20,3,0,8,0x14,0x1f,0x70,0x70,0x70,0x20,0x20,0x20,3,
      2,0x80,1,0xe0,0,2,0,0xf0,0,0x90,0x21,2,1,0x80,0x80,0,0};
    assert(memcmp(command, expected640, sizeof command) == 0);
    const uint8_t synthetic800[10] = {0x81,0x86,2,0x74,0,0,0,0,1,0};
    const uint8_t expected800[31] = {4,0x20,1,4,8,0x14,0x1f,0x70,0x70,0x70,0x20,0x20,0x20,3,
      3,0x20,2,0x58,0,2,1,0x2c,0,0xd8,0x1b,2,1,0x80,0x80,0,0};
    assert(v2u_capture_mode(synthetic800, V2U_MODE_AUTO, command, &layout) == 0);
    assert(memcmp(command, expected800, sizeof command) == 0);
    assert(layout.width == 800 && layout.height == 600 && layout.horizontal_phases == 1);
    assert(v2u_capture_mode(synthetic800, V2U_MODE_640_480, command, &layout) < 0);
    assert(v2u_capture_mode(synthetic800, V2U_MODE_720_400, command, &layout) < 0);
    const uint8_t synthetic1024[10] = {0x82,0x33,3,0x25,0,0,0,0,2,0};
    const uint8_t expected1024[31] = {5,0x40,2,4,8,0x14,0x1f,0x70,0x70,0x70,0x20,0x20,0x20,3,
      4,0,3,0,0,2,1,0x80,1,0x28,0x1c,2,2,0x80,0x80,0,0};
    assert(v2u_capture_mode(synthetic1024, V2U_MODE_AUTO, command, &layout) == 0);
    assert(memcmp(command, expected1024, sizeof command) == 0);
    assert(layout.width == 1024 && layout.height == 768 && layout.horizontal_phases == 2);
    const uint8_t synthetic1024high[10] = {0x51,0x61,3,0x58,0,0,0,0,0,0};
    const uint8_t expected1024high[31] = {5,0x22,0,0x24,8,0x14,0x1f,0x70,0x70,0x70,0x20,0x20,0x20,3,
      4,0,3,0,0,2,1,0x80,0,0xd2,0x3e,2,4,0x80,0x80,0,0};
    assert(v2u_capture_mode(synthetic1024high, V2U_MODE_AUTO, command, &layout) == 0);
    assert(memcmp(command, expected1024high, sizeof command) == 0);
    assert(layout.width == 1024 && layout.height == 768 && layout.horizontal_phases == 4);
    const uint8_t synthetic640high[10] = {0x5b,0xe6,2,0x03,0,0,0,0,0,0};
    const uint8_t expected640high[31] = {3,0x40,0,4,8,0x14,0x1f,0x70,0x70,0x70,0x20,0x20,0x20,3,
      2,0x80,1,0xe0,0,2,0,0xf0,0,0x88,0x19,2,1,0x80,0x80,0,0};
    assert(v2u_capture_mode(synthetic640high, V2U_MODE_AUTO, command, &layout) == 0);
    assert(memcmp(command, expected640high, sizeof command) == 0);
    /* Polarity bit 0x40 is not an exclusion: exact short-blanking rows survive. */
    const uint8_t synthetic800short[10] = {0x82,0x90,2,0x6a,0,0,0,0,0,0};
    const uint8_t expected800short[31] = {3,0xc0,0,4,8,0x14,0x1f,0x70,0x70,0x70,0x20,0x20,0x20,3,
      3,0x20,2,0x58,0,2,1,0x2c,0,0x70,0x0b,2,1,0x80,0x80,0,0};
    assert(v2u_capture_mode(synthetic800short, V2U_MODE_AUTO, command, &layout) == 0);
    assert(memcmp(command, expected800short, sizeof command) == 0);
    const uint8_t synthetic1024short[10] = {0x82,0x7e,3,0x16,0,0,0,0,0,0};
    const uint8_t expected1024short[31] = {4,0xa0,0,4,8,0x14,0x1f,0x70,0x70,0x70,0x20,0x20,0x20,3,
      4,0,3,0,0,2,1,0x80,0,0x70,0x0f,2,2,0x80,0x80,0,0};
    assert(v2u_capture_mode(synthetic1024short, V2U_MODE_AUTO, command, &layout) == 0);
    assert(memcmp(command, expected1024short, sizeof command) == 0);
    /* Vendor accepts scores <3000 and requires at least active height+6 lines. */
    uint8_t near_limit[10] = {0x6b,0x05,1,0xef,0,0,0,0,0,0}; /* R=730,L=495,score2925 */
    assert(v2u_capture_mode(near_limit, V2U_MODE_720_400, command, &layout) == 0);
    near_limit[3] = 0xf0; /* L=496,score3016 */
    assert(v2u_capture_mode(near_limit, V2U_MODE_720_400, command, &layout) < 0);
    memcpy(near_limit, synthetic640, sizeof near_limit);
    near_limit[2] = 1; near_limit[3] = 0xe5; /* L=485: score1600, below active+6 */
    assert(v2u_capture_mode(near_limit, V2U_MODE_640_480, command, &layout) < 0);
    near_limit[0] = 0; near_limit[1] = 1; /* Deliberately absurd R exercises wide score arithmetic. */
    assert(v2u_capture_mode(near_limit, V2U_MODE_AUTO, command, &layout) < 0);
    /* Repeated observations reinitialize geometry after source mode changes. */

    const uint8_t *changes[] = {measured720, synthetic800, synthetic640, synthetic1024, measured720};
    const unsigned widths[] = {720, 800, 640, 1024, 720};
    const unsigned heights[] = {400, 600, 480, 768, 400};
    for (size_t i=0; i<sizeof changes/sizeof changes[0]; ++i) {
        assert(v2u_capture_mode(changes[i], V2U_MODE_AUTO, command, &layout) == 0);
        assert(layout.width == widths[i] && layout.height == heights[i] && layout.vertical_phases == 2);
    }
    assert(v2u_capture_mode(NULL, V2U_MODE_AUTO, command, &layout) < 0);
    assert(v2u_capture_mode(measured720, V2U_MODE_AUTO, NULL, &layout) < 0);
    assert(v2u_capture_mode(measured720, V2U_MODE_AUTO, command, NULL) < 0);
    assert(v2u_capture_mode(measured720, (enum v2u_mode)99, command, &layout) < 0);
    uint8_t invalid[10] = {0};

    struct v2u_home_layout previous_layout = layout;
    memset(command, 0xcc, sizeof command);
    assert(v2u_capture_mode(invalid, V2U_MODE_AUTO, command, &layout) < 0);
    for (size_t i=0; i<sizeof command; ++i) assert(command[i] == 0xcc);
    assert(memcmp(&layout, &previous_layout, sizeof layout) == 0);
    memcpy(invalid, measured720, sizeof invalid);
    invalid[2] = 0x04; invalid[3] = 0xff; /* No retained timing is this tall. */
    assert(v2u_capture_mode(invalid, V2U_MODE_AUTO, command, &layout) < 0);
    assert(v2u_capture_mode(measured720, V2U_MODE_AUTO, command, &layout) == 0);
    const unsigned phases[] = {4,5,8,10,16,20,25,40,50,80,100,200};
    for (size_t i=0;i<sizeof phases/sizeof phases[0];++i) {
        uint8_t before[31]; memcpy(before,command,31);
        assert(v2u_capture_next_phase(command,&layout)==0);
        assert(layout.vertical_phases==phases[i] && layout.wire_bytes==614304);
        assert(command[18]==0 && command[19]==phases[i] && command[25]==phases[i]);
        unsigned rows=400/phases[i]; assert(command[20]==rows/256 && command[21]==rows%256);
        for (size_t j=0;j<31;++j)
            if (j!=18 && j!=19 && j!=20 && j!=21 && j!=25) assert(command[j]==before[j]);
    }
    uint8_t before[31]; memcpy(before,command,31);
    assert(v2u_capture_next_phase(command,&layout)<0 && memcmp(before,command,31)==0);
    puts("Capture mode: vendor timing score, original bytes, partitions, mode changes and no-signal rejection passed");
    return 0;
}
