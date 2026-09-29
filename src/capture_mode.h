#ifndef V2U_CAPTURE_MODE_H
#define V2U_CAPTURE_MODE_H
#include "home_frame.h"
enum v2u_mode { V2U_MODE_AUTO = 0, V2U_MODE_640_480, V2U_MODE_720_400 };
/* Auto uses vendor score/table order for progressive rows <=1024x768, 40..120 Hz.
 * B1 lacks horizontal measurements, so equal vertical timings remain ambiguous. */
int v2u_capture_mode(const uint8_t status[10], enum v2u_mode requested,
                     uint8_t command[31], struct v2u_home_layout *layout);
int v2u_capture_next_phase(uint8_t command[31], struct v2u_home_layout *layout);
#endif
