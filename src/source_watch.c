#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include "source_watch.h"

void v2u_source_watch_destroy(struct v2u_source_watch *watch)
{
    if (watch->timer >= 0) close(watch->timer);
    free(watch->fds);
    watch->timer = -1; watch->fds = NULL; watch->count = 0;
}
int v2u_source_watch_init(struct v2u_source_watch *watch, libusb_context *context)
{
    *watch = (struct v2u_source_watch){.timer=-1};
    const struct libusb_pollfd **usb = libusb_get_pollfds(context);
    if (!usb) return -1;
    size_t count = 0;
    while (usb[count]) ++count;
    watch->fds = calloc(count + 1, sizeof(*watch->fds));
    if (watch->fds) {
        for (size_t i=0; i<count; ++i)
            watch->fds[i] = (struct pollfd){.fd=usb[i]->fd,.events=usb[i]->events};
    }
    libusb_free_pollfds(usb);
    if (!watch->fds) return -1;
    watch->timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (watch->timer < 0) { v2u_source_watch_destroy(watch); return -1; }
    /* Wake for status observation; this never sets a USB or process deadline. */
    const struct itimerspec tick = {.it_interval={0,100000000},.it_value={0,100000000}};
    if (timerfd_settime(watch->timer, 0, &tick, NULL) < 0) {
        v2u_source_watch_destroy(watch); return -1;
    }
    watch->fds[count] = (struct pollfd){.fd=watch->timer,.events=POLLIN};
    watch->count = count + 1;
    return 0;
}
int v2u_source_watch_wait(struct v2u_source_watch *watch, int *probe)
{
    *probe = 0;
    int result;
    /* Wait indefinitely for a USB event or the periodic observation tick. */
    do { result = poll(watch->fds, watch->count, -1); } while (result < 0 && errno == EINTR);
    if (result < 0) return -1;
    if (watch->fds[watch->count-1].revents & POLLIN) {
        uint64_t ticks;
        if (read(watch->timer,&ticks,sizeof ticks) != sizeof ticks) return -1;
        *probe = 1;
    }
    return 0;
}
