#pragma once

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

// Tell TinyUSB we are on ESP32-S3
#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU              OPT_MCU_ESP32S3
#endif

// FreeRTOS
#define CFG_TUSB_OS               OPT_OS_FREERTOS

// Debug level (0=no debug, 1=errors, 2=warnings, 3=info)
#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG            2  // Enable some debug output
#endif

// Alignment required by ESP-IDF
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN        __attribute__((aligned(4)))
#endif

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

// RHPort0: Device, Full-Speed
#ifndef CFG_TUSB_RHPORT0_MODE
#define CFG_TUSB_RHPORT0_MODE     (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#endif

// Control endpoint size
#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE    64
#endif

//------------- CLASS -------------//
// We use two HID interfaces: mouse + keyboard
#define CFG_TUD_HID               2
#define CFG_TUD_CDC               0
#define CFG_TUD_MSC               0
#define CFG_TUD_MIDI              0
#define CFG_TUD_VENDOR            0
#define CFG_TUD_CUSTOM_CLASS      0

// HID buffer sizes (tune as needed)
#define CFG_TUD_HID_EP_BUFSIZE    16

#ifdef __cplusplus
}
#endif