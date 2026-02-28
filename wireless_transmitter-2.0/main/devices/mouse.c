#include "usb/hid_usage_mouse.h"
#include "wifi/msg_types.h"
#include "stddef.h"
#include "esp_err.h"

#define LEN_MIN_MOUSE_REP (sizeof(hid_mouse_input_report_boot_t))
#define LEN_STD_MOUSE_REP 4
#define LEN_HIGH_PRECSICION_MOUSE_REP 6

static inline int8_t clamp16to8(int16_t val) { return (val < -128) ? -128 : ((val > 127) ? 127 : val); }

// Formats and sends esp-now mouse message to receiver.
esp_err_t process_mouse_report(const uint8_t* data, size_t length, uint8_t** formatted_data, uint8_t* formatted_length) {
    
    if (length < LEN_MIN_MOUSE_REP)
        return ESP_FAIL;
    espnow_msg_mouse_t* msg = (espnow_msg_mouse_t*)malloc(sizeof(espnow_msg_mouse_t));
    if (length  <= LEN_STD_MOUSE_REP){
        const hid_mouse_input_report_boot_t* report = (hid_mouse_input_report_boot_t*)data;
        // Fill message
        msg->msg_type    = ESPNOW_MSG_MOUSE;
        msg->buttons     = report->buttons.val;
        msg->x           = report->x_displacement;
        msg->y           = report->y_displacement;
        msg->wheel       = (length == LEN_MIN_MOUSE_REP) ? 0 : data[3];
        msg->pan         = (length >  LEN_STD_MOUSE_REP) ? data[4] : 0;
    }
    else if (length >= LEN_HIGH_PRECSICION_MOUSE_REP) {
        // Bytes are little endian
        // Second byte contains signed bit.
        int16_t dx = (int16_t)((uint16_t)data[1] | ((int16_t)data[2] << 8));
        // Repeat for dy
        int16_t dy = (int16_t)((uint16_t)data[3] | ((int16_t)data[4] << 8));

        // Fill message
        msg->msg_type    = ESPNOW_MSG_MOUSE;
        msg->buttons     = data[0];
        msg->x           = clamp16to8(dx); // Clamp to 8 bits
        msg->y           = clamp16to8(dy); // Clamp to 8 bits
        msg->wheel       = data[5];
        msg->pan         = (length == LEN_HIGH_PRECSICION_MOUSE_REP) ? 0 : data[6];
    }

    *formatted_data = (uint8_t*)msg;
    *formatted_length = sizeof(*msg);
    return ESP_OK;
}