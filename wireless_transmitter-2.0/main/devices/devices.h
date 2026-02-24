#include "usb/hid_host.h"

// starts keyboard-watchdog task
void begin_keyboard_watchdog(void);

// parse a keyboard input-report and return a formatted espnow_message
esp_err_t process_keyboard_report(const uint8_t* data, size_t length,
                                    uint8_t** formatted_data, size_t* formatted_length);

// parse a mouse input-report and return a formatted espnow_message
esp_err_t process_mouse_report(const uint8_t* data, size_t length,
                                uint8_t** formatted_data, size_t* formatted_length);