#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>
#include "capture.h"

static struct libusb_transfer *queue[8];
static int event_fd, status_queries;
static int queued, submitted, setup, fault, cancelled, freed, frame_reads, setups, callbacks;
struct libusb_transfer * LIBUSB_CALL libusb_alloc_transfer(int iso)
{ assert(iso == 0); return calloc(1, sizeof(struct libusb_transfer)); }
void LIBUSB_CALL libusb_free_transfer(struct libusb_transfer *t)
{ for (int i=0; i<queued; ++i) assert(queue[i] != t); free(t); ++freed; }
int LIBUSB_CALL libusb_submit_transfer(struct libusb_transfer *t)
{
    if (setup && queued == 0) { setup=0; frame_reads=0; }
    /* Recovered vendor bound: whole padded rows, maximum 128 KB. */
    assert(t->length == (frame_reads == 4 ? 92064 : 130560));
    assert(t->endpoint == 0x82 && t->timeout == 0);
    assert(setup == (frame_reads != 0));
    if (fault == 3 && frame_reads == 2) return LIBUSB_ERROR_IO;
    assert(queued < 8); queue[queued++] = t; ++submitted; ++frame_reads;
    return 0;
}
int LIBUSB_CALL libusb_cancel_transfer(struct libusb_transfer *t)
{ t->status = LIBUSB_TRANSFER_CANCELLED; ++cancelled;
  if (fault == 7 || fault == 8 || fault == 14) { uint64_t one=1; assert(write(event_fd,&one,sizeof one)==sizeof one); }
  return 0; }
int LIBUSB_CALL libusb_control_transfer(libusb_device_handle *h, uint8_t type,
    uint8_t req, uint16_t val, uint16_t idx, unsigned char *data, uint16_t len, unsigned int wait)
{
    (void)h; assert(val == 0 && idx == 0 && wait == 0);
    if (req == 0xb1) {
        /* Original B1 firmware aborts GPIF; never measure with queued image reads. */
        assert(queued == 0);
        ++status_queries;
        if ((fault == 13 && status_queries == 1) || (fault == 14 && status_queries == 2)) return 0;
        const uint8_t status[10] = {0x6f,0x7d,1,0xc2,0,0xf2,0xa,0xc6,2,0};
        assert(type == 0xc0 && len == 10); memcpy(data,status,10); if ((fault == 4 && setups == 2) || (fault == 6 && setups == 9) || (fault == 7 && setups == 1) || (fault == 9 && setups == 1) || (fault == 10 && status_queries == 3) || (fault == 12 && setups == 2) || ((fault == 13 || fault == 14) && setups == 2)) return LIBUSB_ERROR_NO_DEVICE;
        if ((fault == 15 && setups == 1) || (fault == 12 && (status_queries == 2 || status_queries == 3)) || ((fault == 9 || fault == 10) && status_queries <= 2) || fault == 11) memset(data,0,10);
        return 10;
    }
    assert(type == 0x40 && req == 0xb4 && len == 31);
    assert(frame_reads == 1 && queued == 1 && data[2] == 2);
    ++setups; callbacks=0;
    if (fault == 1 || fault == 5) assert(data[19] == (setups <= 3 ? 2 : 4));
    if (fault == 6) assert(data[19] == (setups <= 3 ? 2 : setups <= 7 ? 4 : 5));
    setup = 1;
    if (fault == 5 && setups <= 3) {
        struct libusb_transfer *t=queue[0]; queued=0;
        t->status=LIBUSB_TRANSFER_COMPLETED; t->actual_length=1536;
        t->callback(t);
    } return fault == 2 ? LIBUSB_ERROR_IO : 31;
}
int LIBUSB_CALL libusb_handle_events_completed(libusb_context *c, int *done)
{
    (void)c; (void)done; assert(queued > 0);
    if ((fault == 7 || fault == 8) && setups == 1) assert(status_queries == 1);
    struct libusb_transfer *t = queue[0];
    memmove(queue,queue+1,(size_t)(--queued)*sizeof(*queue));
    if (t->status != LIBUSB_TRANSFER_CANCELLED) {
        t->status = LIBUSB_TRANSFER_COMPLETED;
        t->actual_length = ((fault == 12 && setups == 1) || (fault == 1 && setups <= 3) || (fault == 6 && (setups <= 3 || (setups >= 5 && setups <= 7)))) && callbacks == 0 ? 1536 : t->length;
        for (int i=0;i<t->actual_length;i+=2) { t->buffer[i]=0; t->buffer[i+1]=0xf8; }
    }
    ++callbacks; t->callback(t); return 0;
}
const struct libusb_pollfd ** LIBUSB_CALL libusb_get_pollfds(libusb_context *c)
{
    (void)c; static struct libusb_pollfd fd;
    fd.fd=event_fd; fd.events=POLLIN;
    const struct libusb_pollfd **fds=calloc(2,sizeof(*fds)); assert(fds); fds[0]=&fd; return fds;
}
void LIBUSB_CALL libusb_free_pollfds(const struct libusb_pollfd **fds) { free(fds); }
int LIBUSB_CALL libusb_handle_events_timeout_completed(libusb_context *c, struct timeval *tv, int *done)
{
    assert(tv->tv_sec == 0 && tv->tv_usec == 0);
    struct pollfd fd={.fd=event_fd,.events=POLLIN};
    if (queued && poll(&fd,1,0)>0) return libusb_handle_events_completed(c,done);
    return 0;
}
int main(void)
{
    for (fault=0; fault<=15; ++fault) {
        queued=submitted=setup=cancelled=freed=frame_reads=setups=callbacks=0;
        status_queries=0; event_fd=eventfd((fault == 7 || fault == 8) ? 0 : 1,EFD_NONBLOCK|EFD_CLOEXEC); assert(event_fd >= 0);
        pid_t producer=-1;
        if (fault == 7 || fault == 8) {
            producer=fork(); assert(producer >= 0);
            if (!producer) {
                /* Delayed hardware completion crosses two old observation ticks. */
                struct timespec delay={0,250000000}; assert(nanosleep(&delay,NULL)==0);
                uint64_t one=1; assert(write(event_fd,&one,sizeof one)==sizeof one);
                _exit(0);
            }
        }
        FILE *f=tmpfile(); assert(f);
        int result=v2u_capture_ppm(NULL,NULL,fault == 10 ? V2U_MODE_640_480 : V2U_MODE_AUTO,f,fault == 4 || fault == 6 || fault == 7 || fault == 9 || fault == 10 || fault == 12 || fault == 13 || fault == 14 || fault == 15);
        assert(queued == 0);
        if (fault <= 1 || fault == 5) {
            assert(result == 0 && submitted == (fault == 5 ? 8 : fault ? 20 : 5) && setups == (fault ? 4 : 1));
            assert(ftell(f) == 15 + 720*400*3);
            rewind(f); char header[16]={0}; assert(fread(header,1,15,f)==15);
            assert(strcmp(header,"P6\n720 400\n255\n")==0);
            assert(fgetc(f)==255 && fgetc(f)==0 && fgetc(f)==0);
        } else if (fault == 4) {
            assert(result < 0 && setups == 2 && submitted == 10 && cancelled == 0);
            assert(ftell(f) == 2*(15+720*400*3));
            assert(fseek(f,15+720*400*3,SEEK_SET)==0);
            char header[16]={0}; assert(fread(header,1,15,f)==15);
            assert(strcmp(header,"P6\n720 400\n255\n")==0);
        } else if (fault == 6) {
            assert(result < 0 && setups == 9 && submitted == 45);
            assert(ftell(f)==3*(15+720*400*3));
            for (int i=0;i<3;++i) {
                assert(fseek(f,i*(15+720*400*3),SEEK_SET)==0);
                char header[16]={0}; assert(fread(header,1,15,f)==15);
                assert(strcmp(header,"P6\n720 400\n255\n")==0);
            }
        } else if (fault == 7 || fault == 8) {
            assert(fault == 7 ? result < 0 : result == 0);
            assert(setups == 1 && cancelled == 0 && submitted == 5);
            assert(ftell(f)==15+720*400*3);
            assert(status_queries == (fault == 7 ? 2 : 1));
            rewind(f); assert(fseek(f,15,SEEK_SET)==0);
            assert(fgetc(f)==255 && fgetc(f)==0 && fgetc(f)==0);
        } else if (fault == 9 || fault == 10) {
            assert(result < 0 && cancelled == 0);
            assert(ftell(f)==(fault == 9 ? 2*(15+720*400*3) : 15+640*480*3));
            rewind(f); char header[16]={0}; assert(fread(header,1,15,f)==15);
            assert(strcmp(header,fault == 9 ? "P6\n720 400\n255\n" : "P6\n640 480\n255\n")==0);
        } else if (fault == 11) {
            assert(result < 0 && submitted == 0 && ftell(f)==0);
        } else if (fault == 12) {
            assert(result == V2U_CAPTURE_REOPEN && setups == 1 && cancelled == 4);
            assert(ftell(f)==15+720*400*3);
        } else if (fault == 13) {
            assert(result < 0 && setups == 2 && cancelled == 0);
            assert(ftell(f)==3*(15+720*400*3));
        } else if (fault == 14) {
            assert(result == V2U_CAPTURE_REOPEN && setups == 1 && cancelled == 0);
            assert(ftell(f)==2*(15+720*400*3));
        } else if (fault == 15) {
            assert(result == V2U_CAPTURE_REOPEN && setups == 1 && cancelled == 0);
            assert(ftell(f)==2*(15+720*400*3));
        } else { assert(result < 0 && ftell(f) == 0 && cancelled > 0); }

        if (producer > 0) { int child_status; assert(waitpid(producer,&child_status,0)==producer); assert(WIFEXITED(child_status) && WEXITSTATUS(child_status)==0); }
        fclose(f); close(event_fd);
    }
    puts("Capture transport: row-aligned reads, B4 order, short frame rejection and cancellation passed");
}
