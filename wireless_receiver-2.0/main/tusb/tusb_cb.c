#include "devices.h"
#include "esp_log.h"
#include "tusb.h"
#include "tusb_cb.h"
#include "tusb_device_common.h"
#include <string.h>

// Configuration Descriptors
#define POLLING_RATE 1 // The polling interval (in ms) at which the host will check for new data (Try 1-4ms)
#define MA_CURR_DRAW 100
#define NUM_INFS 3
#define EPNUM_HID_MOUSE     0x81
#define EPNUM_HID_KEYBOARD  0x82
#define EPNUM_HID_GAMEPAD   0x83
#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + (NUM_INFS * TUD_HID_DESC_LEN))

// Device Descriptor
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,  // Will be defined at interface level
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x303A,  // Defining as an Espressif device should assist in driver compatibility
    .idProduct          = 0x4010,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// HID Report Descriptors
uint8_t const desc_hid_report_mouse[]       = { TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(HID_MOUSE_REPORT_ID)) };
uint8_t const desc_hid_report_keyboard[]    = { TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_KEYBOARD_REPORT_ID)) };
uint8_t const desc_hid_report_gamepad[]     = { TUD_HID_REPORT_DESC_GAMEPAD(HID_REPORT_ID(HID_GAMEPAD_REPORT_ID)) };

uint8_t const desc_configuration[] = {
    // Config: 1 config, 3 interfaces, no string, total length, remote wakeup, 100mA
    TUD_CONFIG_DESCRIPTOR(
        1,
        NUM_INFS,
        0,
        CONFIG_TOTAL_LEN,
        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,
        MA_CURR_DRAW
    ),

    // Mouse Interface
    TUD_HID_DESCRIPTOR(
        HID_MOUSE_ITF_NUM,
        0,
        HID_ITF_PROTOCOL_MOUSE,
        sizeof(desc_hid_report_mouse),
        EPNUM_HID_MOUSE,
        CFG_TUD_HID_EP_BUFSIZE,
        POLLING_RATE
    ),

    // Keyboard Interface
    TUD_HID_DESCRIPTOR(
        HID_KEYBOARD_ITF_NUM,
        0,
        HID_ITF_PROTOCOL_KEYBOARD,
        sizeof(desc_hid_report_keyboard),
        EPNUM_HID_KEYBOARD,
        CFG_TUD_HID_EP_BUFSIZE,
        POLLING_RATE
    ),

    // Gamepad Interface  
    TUD_HID_DESCRIPTOR(
        HID_GAMEPAD_ITF_NUM,
        0,
        HID_ITF_PROTOCOL_NONE,
        sizeof(desc_hid_report_gamepad),
        EPNUM_HID_GAMEPAD,
        CFG_TUD_HID_EP_BUFSIZE,
        POLLING_RATE
    )
};

// String Descriptors
static char const* string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },  // 0: Language (English)
    "Espressif",                    // 1: Manufacturer
    "Wireless Adapter",             // 2: Product
    "123456"                        // 3: Serial
};



// HID Callbacks
// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
// Leave empty, devices will send reports according to their polling rate
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen){
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
// ! ! ! TO IMPLEMENT ! ! !
// Requires: Host -> Device Communication
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
}

// Invoked when a HID report is completed
// Notifies appropriate queue that their particular HID instance will be ready soon
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len){
    (void)report;
    (void)len;
    notify_nst_task(instance);
}

// OS callback requests for Device Descriptor ^^^
uint8_t const* tud_descriptor_device_cb(void){
    return (uint8_t const *) &desc_device;
}

// OS callback requests the correct report descriptor for each HID instance ^^^
uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance){
    switch (instance){
        case HID_MOUSE_INSTANCE:
            return desc_hid_report_mouse;
        case HID_KEYBOARD_INSTANCE:
            return desc_hid_report_keyboard;
        case HID_GAMEPAD_INSTANCE:
            return desc_hid_report_gamepad;
        default:
            return 0;
    }
}

// OS callback requests Configuration Descriptor ^^^
uint8_t const* tud_descriptor_configuration_cb(uint8_t index)
{
    (void) index; // We have only defined 1 Config Desc!
    return desc_configuration;
}

static uint16_t _desc_str[32];

// OS callback requests String Descriptor^^^
// Converts UTF-8 descriptor to required UTF-16 format
const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void) langid;
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } 
    else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) 
            return NULL;
        const char *str = string_desc_arr[index];
        chr_count = strlen(str);

        if (chr_count > 31) 
            chr_count = 31;
        for (uint8_t i = 0; i < chr_count; i++){
            _desc_str[1 + i] = str[i];
        }
    }
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
    return _desc_str;
}