#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

// Ensure TUSB knows we are on ESP32S3
#define CFG_TUSB_MCU                OPT_MCU_ESP32S3
// Set OTG HOST MODE
#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_HOST    // ← KEY DIFFERENCE! 
#define CFG_TUSB_OS                 OPT_OS_FREERTOS

// Set debug level
// (0=no debug, 1=errors, 2=warnings, 3=info)
#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG              0
#endif

// USB Host stack configuration
#define CFG_TUSB_HOST_DEVICE_MAX    (CFG_TUH_HUB ?  4 : 1) // how many devices

//--------------------------------------------------------------------
// HOST CONFIGURATION
//--------------------------------------------------------------------

// Size of buffer to hold descriptors and other data for enumeration
#define CFG_TUH_ENUMERATION_BUFSIZE 256

// Number of hub devices (usually 0 for simple setups)
#define CFG_TUH_HUB                 0

// Max number of HID devices
#define CFG_TUH_HID                 1  // 1 keyboard or 1 mouse

// Number of endpoints per HID device
#define CFG_TUH_HID_EPIN            4
#define CFG_TUH_HID_EPOUT           4

// CDC, MSC, and Vendor USB class support not needed
#define CFG_TUH_CDC                 0
#define CFG_TUH_MSC                 0
#define CFG_TUH_VENDOR              0

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */