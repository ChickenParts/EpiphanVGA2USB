#ifndef V2U_CAPTURE_MODE_H
#define V2U_CAPTURE_MODE_H
#include "home_frame.h"
enum v2u_mode { V2U_MODE_AUTO = 0, V2U_MODE_640_480, V2U_MODE_720_400 };
/* Auto is deliberately limited to the uniquely identified 720x400 timing. */
int v2u_capture_mode(const uint8_t status[10], enum v2u_mode requested,
                     uint8_t command[31], struct v2u_home_layout *layout);
#endif
