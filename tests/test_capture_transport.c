#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "capture.h"

static struct libusb_transfer *queue[8];
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
{ t->status = LIBUSB_TRANSFER_CANCELLED; ++cancelled; return 0; }
int LIBUSB_CALL libusb_control_transfer(libusb_device_handle *h, uint8_t type,
    uint8_t req, uint16_t val, uint16_t idx, unsigned char *data, uint16_t len, unsigned int wait)
{
    (void)h; assert(val == 0 && idx == 0 && wait == 0);
    if (req == 0xb1) {
        const uint8_t status[10] = {0x6f,0x7d,1,0xc2,0,0xf2,0xa,0xc6,2,0};
        assert(type == 0xc0 && len == 10); memcpy(data,status,10); if ((fault == 4 && setups == 2) || (fault == 6 && setups == 9)) memset(data,0,10); return 10;
    }
    assert(type == 0x40 && req == 0xb4 && len == 31);
    assert(frame_reads == 1 && queued == 1 && data[2] == 2);
    ++setups; callbacks=0;
    if (fault == 1 || fault == 5) assert(data[19] == (setups <= 3 ? 2 : 4));
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
    struct libusb_transfer *t = queue[0];
    memmove(queue,queue+1,(size_t)(--queued)*sizeof(*queue));
    if (t->status != LIBUSB_TRANSFER_CANCELLED) {
        t->status = LIBUSB_TRANSFER_COMPLETED;
        t->actual_length = ((fault == 1 && setups <= 3) || (fault == 6 && (setups <= 3 || (setups >= 5 && setups <= 7)))) && callbacks == 0 ? 1536 : t->length;
        for (int i=0;i<t->actual_length;i+=2) { t->buffer[i]=0; t->buffer[i+1]=0xf8; }
    }
    ++callbacks; t->callback(t); return 0;
}
int main(void)
{
    for (fault=0; fault<=6; ++fault) {
        queued=submitted=setup=cancelled=freed=frame_reads=setups=callbacks=0;
        FILE *f=tmpfile(); assert(f);
        int result=v2u_capture_ppm(NULL,NULL,V2U_MODE_AUTO,f,fault == 4 || fault == 6);
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
        } else { assert(result < 0 && ftell(f) == 0 && cancelled > 0); }
        fclose(f);
    }
    puts("Capture transport: row-aligned reads, B4 order, short frame rejection and cancellation passed");
}
