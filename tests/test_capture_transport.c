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
#include "stream_frame.h"

static struct libusb_transfer *queue[32];
static int event_fd, status_queries;
static struct v2u_home_layout expected_layout;
static uint8_t expected_command[31];
static uint8_t *wire_start;
static const uint8_t timing720[10]={0x6f,0x7d,1,0xc2,0,0xf2,0xa,0xc6,2,0};
static const uint8_t timing640[10]={0x82,0x35,2,0x0d,0,0,0,0,3,0};
static const uint8_t timing800[10]={0x81,0x86,2,0x74,0,0,0,0,1,0};
static const uint8_t timing1024[10]={0x82,0x33,3,0x25,0,0,0,0,2,0};
static const uint8_t near_limit[10]={0x6b,0x05,1,0xef,0,0,0,0,0,0};

static uint16_t source_pixel(unsigned x, unsigned y, unsigned width, unsigned height)
{
    return (uint16_t)((((x/11 + width/16) % 32) << 11) |
                      (((y/7 + height/16) % 64) << 5) |
                      ((x + y + height) % 32));
}
static int queued, submitted, setup, fault, cancelled, freed, frame_reads, setups, callbacks;
struct libusb_transfer * LIBUSB_CALL libusb_alloc_transfer(int iso)
{ assert(iso == 0); return calloc(1, sizeof(struct libusb_transfer)); }
void LIBUSB_CALL libusb_free_transfer(struct libusb_transfer *t)
{ for (int i=0; i<queued; ++i) assert(queue[i] != t); free(t); ++freed; }
int LIBUSB_CALL libusb_submit_transfer(struct libusb_transfer *t)
{
    if (setup && queued == 0) { setup=0; frame_reads=0; }
    /* Recovered vendor bound: whole padded rows, maximum 128 KB. */
size_t block=(131072/expected_layout.segment_stride)*expected_layout.segment_stride;
size_t offset=(size_t)frame_reads*block;
size_t remaining=expected_layout.wire_bytes-offset;
assert(offset < expected_layout.wire_bytes);
assert(t->length == (int)(remaining < block ? remaining : block));
if (!frame_reads) wire_start=t->buffer;
assert(t->buffer == wire_start+offset);
    assert(t->endpoint == 0x82 && t->timeout == 0);
    assert(setup == (frame_reads != 0));
    if (fault == 3 && frame_reads == 2) return LIBUSB_ERROR_IO;
    assert(queued < 32); queue[queued++] = t; ++submitted; ++frame_reads;
    return 0;
}
int LIBUSB_CALL libusb_cancel_transfer(struct libusb_transfer *t)
{ t->status = LIBUSB_TRANSFER_CANCELLED; ++cancelled;
  if (fault == 7 || fault == 8 || fault == 14 || fault >= 16) { uint64_t one=1; assert(write(event_fd,&one,sizeof one)==sizeof one); }
  return 0; }
int LIBUSB_CALL libusb_control_transfer(libusb_device_handle *h, uint8_t type,
    uint8_t req, uint16_t val, uint16_t idx, unsigned char *data, uint16_t len, unsigned int wait)
{
    (void)h; assert(val == 0 && idx == 0 && wait == 0);
    if (req == 0xb1) {
        /* Original B1 firmware aborts GPIF; never measure with queued image reads. */
        assert(queued == 0);
        ++status_queries;
if (fault >= 16 && fault <= 18 && status_queries == 2) assert(cancelled == 5);
if (fault >= 19) {
    assert(type == 0xc0 && len == 10);
    const uint8_t *timing=timing720;
    enum v2u_mode requested=V2U_MODE_AUTO;
    if (fault == 19) {
        if (status_queries == 3) {
            assert(cancelled == 5 && freed == submitted);
            memset(data,0,10); return 10;
        }
        if (status_queries == 4) timing=timing640;
        if (status_queries == 5) timing=timing800;
        if (status_queries == 6) timing=timing1024;
        if (status_queries == 8) return LIBUSB_ERROR_NO_DEVICE;
    } else if (fault == 20) {
        timing=near_limit; requested=V2U_MODE_720_400;
        if (status_queries == 2) assert(cancelled == 5 && freed == submitted);
        if (status_queries == 3) return LIBUSB_ERROR_NO_DEVICE;
    } else if (fault == 21) {
        if (status_queries > 1) {
            assert(cancelled == 5*(status_queries-1) && freed == submitted);
        }
        if (status_queries == 5) return LIBUSB_ERROR_NO_DEVICE;
    }
    memcpy(data,timing,10);
    assert(v2u_capture_mode(timing,requested,expected_command,&expected_layout)==0);
    return 10;
}
        if (fault == 17 && status_queries == 2) {
            const uint8_t graphics[10]={0x82,0x35,2,0x0d,0,0,0,0,3,0};
            memcpy(data,graphics,10); return 10;
        }
        if ((fault == 13 && status_queries == 1) || (fault == 14 && status_queries == 2)) return 0;
        const uint8_t status[10] = {0x6f,0x7d,1,0xc2,0,0xf2,0xa,0xc6,2,0};
        assert(type == 0xc0 && len == 10); memcpy(data,status,10); if ((fault == 4 && setups == 2) || (fault == 6 && setups == 9) || (fault == 7 && setups == 1) || (fault == 9 && setups == 1) || (fault == 10 && status_queries == 3) || (fault == 12 && setups == 2) || ((fault == 13 || fault == 14) && setups == 2)) return LIBUSB_ERROR_NO_DEVICE;
        if ((fault == 16 && status_queries == 2) || (fault == 15 && setups == 1) || (fault == 12 && (status_queries == 2 || status_queries == 3)) || ((fault == 9 || fault == 10) && status_queries <= 2) || fault == 11) memset(data,0,10);
        return 10;
    }
    assert(type == 0x40 && req == 0xb4 && len == 31);
assert(frame_reads == 1 && queued == 1);
if (fault >= 19) {
    assert(memcmp(data,expected_command,31)==0);
    assert(data[14]*256u+data[15] == expected_layout.width);
    assert(data[16]*256u+data[17] == expected_layout.height);
    assert(data[26] == expected_layout.horizontal_phases);
    assert(data[19] == 2 && data[25] == 2);
} else assert(data[2] == 2);
    ++setups; callbacks=0;
    if (fault == 1 || fault == 5) assert(data[19] == (setups <= 3 ? 2 : 4));
    if (fault == 6) assert(data[19] == (setups <= 3 ? 2 : setups <= 7 ? 4 : 5));
setup = 1;
if ((fault == 19 && setups == 2) || (fault == 20 && setups == 1) || fault == 21) {
    uint64_t ticks;
    /* Remove callback readiness so the real source-watch timer observes a stall. */
    while (read(event_fd,&ticks,sizeof ticks)==sizeof ticks) {}
}
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
for (int i=0;i<t->actual_length;i+=2) {
    uint16_t pixel=0xf800;
    if (fault >= 19) {
        size_t offset=(size_t)(t->buffer-wire_start)+(size_t)i;
        size_t segment=offset/expected_layout.segment_stride;
        unsigned sample=(unsigned)(offset%expected_layout.segment_stride)/2;
        unsigned lines=expected_layout.height/expected_layout.vertical_phases;
        unsigned r=(unsigned)(segment%expected_layout.height);
        unsigned y=(r%lines)*expected_layout.vertical_phases+r/lines;
        unsigned x=(unsigned)(segment/expected_layout.height)+
                   sample*expected_layout.horizontal_phases;
        pixel=x < expected_layout.width ? source_pixel(x,y,expected_layout.width,expected_layout.height) : 0xa5a5;
    }
    t->buffer[i]=(uint8_t)pixel; t->buffer[i+1]=(uint8_t)(pixel>>8);
}
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
static void check_canvas(FILE *output, unsigned width, unsigned height, int black)
{
    char header[16]={0};
    assert(fread(header,1,15,output)==15);
    assert(strcmp(header,"P6\n720 480\n255\n")==0);
    for (unsigned y=0;y<V2U_STREAM_HEIGHT;++y) {
        for (unsigned x=0;x<V2U_STREAM_WIDTH;++x) {
            uint16_t pixel=black ? 0 : source_pixel(x*width/V2U_STREAM_WIDTH,
                                                    y*height/V2U_STREAM_HEIGHT,width,height);
            unsigned r=(pixel>>11)&31, g=(pixel>>5)&63, b=pixel&31;
            assert(fgetc(output)==(int)((r<<3)|(r>>2)));
            assert(fgetc(output)==(int)((g<<2)|(g>>4)));
            assert(fgetc(output)==(int)((b<<3)|(b>>2)));
        }
    }
}

int main(void)
{
    for (fault=0; fault<=21; ++fault) {
        queued=submitted=setup=cancelled=freed=frame_reads=setups=callbacks=0;
status_queries=0;
assert(v2u_capture_mode(timing720,V2U_MODE_AUTO,expected_command,&expected_layout)==0);
event_fd=eventfd((fault == 7 || fault == 8 || (fault >= 16 && fault != 19)) ? 0 : 1,EFD_NONBLOCK|EFD_CLOEXEC); assert(event_fd >= 0);
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
        int continuous=fault == 4 || fault == 6 || fault == 7 || fault == 9 || fault == 10 || fault == 12 || fault == 13 || fault == 14 || fault == 15 || fault == 16 || fault == 17 || fault >= 19;
        long packet=15+720*(continuous ? 480 : 400)*3;
if (fault == 20) {
    uint8_t automatic[31]; struct v2u_home_layout automatic_layout;
    assert(v2u_capture_mode(near_limit,V2U_MODE_AUTO,automatic,&automatic_layout)==0);
    assert(v2u_capture_mode(near_limit,V2U_MODE_720_400,expected_command,&expected_layout)==0);
    assert(memcmp(automatic,expected_command,31)!=0);
}
int result=v2u_capture_ppm(NULL,NULL,fault == 10 ? V2U_MODE_640_480 :
                            fault == 20 ? V2U_MODE_720_400 : V2U_MODE_AUTO,f,continuous);
        assert(queued == 0);
if (fault == 19) {
    assert(result == V2U_CAPTURE_REOPEN && setups == 2 && cancelled == 5);
    assert(status_queries == 3 && submitted == 10 && freed == submitted);
    assert(ftell(f) == 2*packet);
    /* A reopened session retains the same output stream and redetects each mode. */
    result=v2u_capture_ppm(NULL,NULL,V2U_MODE_AUTO,f,1);
    assert(result < 0 && result != V2U_CAPTURE_REOPEN);
    assert(status_queries == 8 && setups == 6 && cancelled == 5 && queued == 0);
    assert(submitted == 43 && freed == submitted);
    assert(ftell(f) == 6*packet);
    rewind(f);
    check_canvas(f,720,400,0);
    check_canvas(f,720,400,1);
    check_canvas(f,640,480,0);
    check_canvas(f,800,600,0);
    check_canvas(f,1024,768,0);
    check_canvas(f,720,400,0);
    assert(fgetc(f)==EOF);
} else if (fault == 20) {
    assert(result < 0 && result != V2U_CAPTURE_REOPEN);
    assert(setups == 2 && cancelled == 5 && submitted == 10 && freed == submitted);
    assert(status_queries == 3 && ftell(f) == packet);
    rewind(f); check_canvas(f,720,400,0); assert(fgetc(f)==EOF);
} else if (fault == 21) {
    assert(result == V2U_CAPTURE_REOPEN);
    assert(setups == 3 && cancelled == 15 && submitted == 15 && freed == submitted);
    assert(status_queries == 4 && ftell(f) == packet);
    rewind(f); check_canvas(f,720,400,1); assert(fgetc(f)==EOF);
} else if (fault <= 1 || fault == 5) {
            assert(result == 0 && submitted == (fault == 5 ? 8 : fault ? 20 : 5) && setups == (fault ? 4 : 1));
            assert(ftell(f) == packet);
            rewind(f); char header[16]={0}; assert(fread(header,1,15,f)==15);
            assert(strcmp(header,(continuous ? "P6\n720 480\n255\n" : "P6\n720 400\n255\n"))==0);
            assert(fgetc(f)==255 && fgetc(f)==0 && fgetc(f)==0);
        } else if (fault == 4) {
            assert(result < 0 && setups == 2 && submitted == 10 && cancelled == 0);
            assert(ftell(f) == 2*(packet));
            assert(fseek(f,packet,SEEK_SET)==0);
            char header[16]={0}; assert(fread(header,1,15,f)==15);
            assert(strcmp(header,(continuous ? "P6\n720 480\n255\n" : "P6\n720 400\n255\n"))==0);
        } else if (fault == 6) {
            assert(result < 0 && setups == 9 && submitted == 45);
            assert(ftell(f)==3*(packet));
            for (int i=0;i<3;++i) {
                assert(fseek(f,i*(packet),SEEK_SET)==0);
                char header[16]={0}; assert(fread(header,1,15,f)==15);
                assert(strcmp(header,(continuous ? "P6\n720 480\n255\n" : "P6\n720 400\n255\n"))==0);
            }
        } else if (fault == 7 || fault == 8) {
            assert(fault == 7 ? result < 0 : result == 0);
            assert(setups == 1 && cancelled == 0 && submitted == 5);
            assert(ftell(f)==packet);
            assert(status_queries == (fault == 7 ? 2 : 1));
            rewind(f); assert(fseek(f,15,SEEK_SET)==0);
            assert(fgetc(f)==255 && fgetc(f)==0 && fgetc(f)==0);
        } else if (fault == 9 || fault == 10) {
            assert(result < 0 && cancelled == 0);
            assert(ftell(f)==(fault == 9 ? 2*(packet) : packet));
            rewind(f); char header[16]={0}; assert(fread(header,1,15,f)==15);
            assert(strcmp(header,"P6\n720 480\n255\n")==0);
        } else if (fault == 11) {
            assert(result < 0 && submitted == 0 && ftell(f)==0);
        } else if (fault == 12) {
            assert(result == V2U_CAPTURE_REOPEN && setups == 1 && cancelled == 4);
            assert(ftell(f)==packet);
        } else if (fault == 13) {
            assert(result < 0 && setups == 2 && cancelled == 0);
            assert(ftell(f)==3*(packet));
        } else if (fault == 14) {
            assert(result == V2U_CAPTURE_REOPEN && setups == 1 && cancelled == 0);
            assert(ftell(f)==2*(packet));
        } else if (fault == 16 || fault == 17) {
            assert(result == V2U_CAPTURE_REOPEN && setups == 1 && cancelled == 5);
            assert(status_queries == 2 && ftell(f)==packet);
        } else if (fault == 18) {
            assert(result == 0 && setups == 2 && cancelled == 5);
            assert(status_queries == 2 && ftell(f)==packet);
        } else if (fault == 15) {
            assert(result == V2U_CAPTURE_REOPEN && setups == 1 && cancelled == 0);
            assert(ftell(f)==2*(packet));
        } else { assert(result < 0 && ftell(f) == 0 && cancelled > 0); }

        if (producer > 0) { int child_status; assert(waitpid(producer,&child_status,0)==producer); assert(WIFEXITED(child_status) && WEXITSTATUS(child_status)==0); }
        fclose(f); close(event_fd);
    }
    puts("Capture transport: row-aligned reads, B4 order, cancellation, mode-change canvases and preset reacquisition passed");
}
