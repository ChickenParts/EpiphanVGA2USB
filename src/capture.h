#ifndef V2U_CAPTURE_H
#define V2U_CAPTURE_H
#include <libusb.h>
#include <stdio.h>
#include "capture_mode.h"
int v2u_capture_ppm(libusb_context *context, libusb_device_handle *handle,
                     enum v2u_mode requested, FILE *output, int continuous);
#endif
