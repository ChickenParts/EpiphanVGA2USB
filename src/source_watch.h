#ifndef V2U_SOURCE_WATCH_H
#define V2U_SOURCE_WATCH_H
#include <libusb.h>
#include <poll.h>
struct v2u_source_watch { int timer; struct pollfd *fds; nfds_t count; };
int v2u_source_watch_init(struct v2u_source_watch *watch, libusb_context *context, unsigned cadence_ms);
int v2u_source_watch_wait(struct v2u_source_watch *watch, int *probe);
void v2u_source_watch_destroy(struct v2u_source_watch *watch);
#endif
