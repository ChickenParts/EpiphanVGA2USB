#include <assert.h>
#include <string.h>
#include <stdlib.h>
#define main v2u_program
#include "main.c"
#undef main

static int opened, claimed, opens, closes, claims, releases, captures, scenario;
static libusb_device *device = (libusb_device *)1;
static libusb_device *devices[2];
static struct libusb_endpoint_descriptor endpoint = {
    .bEndpointAddress=0x82, .bmAttributes=LIBUSB_TRANSFER_TYPE_BULK, .wMaxPacketSize=512
};
static struct libusb_interface_descriptor alternate = {
    .bInterfaceNumber=0, .bAlternateSetting=0, .bNumEndpoints=1, .endpoint=&endpoint
};
static struct libusb_interface interface = {.altsetting=&alternate,.num_altsetting=1};
static struct libusb_config_descriptor config = {.bNumInterfaces=1,.interface=&interface};

int LIBUSB_CALL libusb_init(libusb_context **context)
{ *context=(libusb_context *)1; return 0; }
void LIBUSB_CALL libusb_exit(libusb_context *context) { assert(context==(void *)1); }
ssize_t LIBUSB_CALL libusb_get_device_list(libusb_context *context, libusb_device ***list)
{ (void)context; devices[0]=device; *list=devices; return 1; }
void LIBUSB_CALL libusb_free_device_list(libusb_device **list, int unref)
{ assert(list==devices && unref==1); }
int LIBUSB_CALL libusb_get_device_descriptor(libusb_device *d, struct libusb_device_descriptor *desc)
{ assert(d==device); *desc=(struct libusb_device_descriptor){.idVendor=0x5555,.idProduct=0x1110,.bcdDevice=firmware_version}; return 0; }
int LIBUSB_CALL libusb_get_port_numbers(libusb_device *d, uint8_t *ports, int capacity)
{ assert(d==device && capacity>=2); ports[0]=12; ports[1]=2; return 2; }
uint8_t LIBUSB_CALL libusb_get_bus_number(libusb_device *d) { assert(d==device); return 1; }
uint8_t LIBUSB_CALL libusb_get_device_address(libusb_device *d) { assert(d==device); return 24; }
int LIBUSB_CALL libusb_get_device_speed(libusb_device *d) { assert(d==device); return LIBUSB_SPEED_HIGH; }
const char * LIBUSB_CALL libusb_error_name(int code) { (void)code; return "simulated error"; }
int LIBUSB_CALL libusb_open(libusb_device *d, libusb_device_handle **handle)
{ assert(d==device && !opened && !claimed); ++opens; if (scenario==1 && opens==2) return LIBUSB_ERROR_NO_DEVICE; opened=1; *handle=(libusb_device_handle *)(size_t)opens; return 0; }
void LIBUSB_CALL libusb_close(libusb_device_handle *handle)
{ assert(handle==(void *)(size_t)opens && opened && !claimed); ++closes; opened=0; }
int LIBUSB_CALL libusb_claim_interface(libusb_device_handle *handle, int number)
{ assert(handle==(void *)(size_t)opens && opened && !claimed && number==0); ++claims; claimed=1; return 0; }
int LIBUSB_CALL libusb_release_interface(libusb_device_handle *handle, int number)
{ assert(handle==(void *)(size_t)opens && opened && claimed && number==0); ++releases; claimed=0; return 0; }
int LIBUSB_CALL libusb_get_active_config_descriptor(libusb_device *d, struct libusb_config_descriptor **c)
{ assert(d==device); *c=&config; return 0; }
void LIBUSB_CALL libusb_free_config_descriptor(struct libusb_config_descriptor *c) { assert(c==&config); }
int LIBUSB_CALL libusb_control_transfer(libusb_device_handle *handle, uint8_t type, uint8_t request,
    uint16_t value, uint16_t index, unsigned char *data, uint16_t length, unsigned int timeout)
{ (void)handle;(void)type;(void)request;(void)value;(void)index;(void)data;(void)length;assert(timeout==0);abort(); }
int v2u_capture_ppm(libusb_context *context, libusb_device_handle *handle, enum v2u_mode mode, FILE *output, int continuous)
{
    assert(context==(void *)1 && handle==(void *)(size_t)opens && opened && claimed);
    assert(mode==V2U_MODE_AUTO); assert(scenario==2 ? output!=stdout && !continuous : output==stdout && continuous);
    ++captures;
    return captures==1 ? V2U_CAPTURE_REOPEN : 0;
}
int main(void)
{
    for (scenario=0; scenario<4; ++scenario) {
        opened=claimed=opens=closes=claims=releases=captures=0;
        char *stream[]={"vga2usb-native","--stream","--port","1-12.2",NULL};
        char *snapshot[]={"vga2usb-native","--capture","/dev/null","--port","1-12.2",NULL};
        char *combined[]={"vga2usb-native","--capture","/dev/null","--stream","--port","1-12.2",NULL};
        int result=v2u_program(scenario==3 ? 6 : scenario==2 ? 5 : 4, scenario==3 ? combined : scenario==2 ? snapshot : stream);
        if (scenario==0 || scenario==3) {
            assert(result==0);
            assert(captures==2 && opens==2 && closes==2 && claims==2 && releases==2);
        } else if (scenario==1) {
            assert(result==1 && opens==2 && captures==1);
            assert(closes==1 && claims==1 && releases==1);
        } else {
            assert(result==1 && opens==1 && captures==1);
            assert(closes==1 && claims==1 && releases==1);
        }
        assert(!opened && !claimed);
    }
    puts("Capture session: reopen on stream loss, reopen failure and strict one-shot cleanup passed");
}
