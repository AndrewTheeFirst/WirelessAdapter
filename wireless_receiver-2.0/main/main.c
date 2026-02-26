#include <stdio.h>
#include "msg_types.h"
#include "wifi.h"
#include "devices.h"
#include "esp_log.h"
#include "tusb.h"
#include "hardware.h"

static const char* TAG = "USB_RECEIVER // main.c";

espnow_msg_test_t packet = {
    .msg_type = ESPNOW_MSG_TEST_END,
    .buffer = {0}
};

// message callback to be invoked when data is received -- referenced in wifi.c
// Routes messages to their repective queues
void app_process_message(const uint8_t* msg){
    espnow_message_t* esp_msg = (espnow_message_t*)msg;
    switch (esp_msg->msg_type) {
        case ESPNOW_MSG_MOUSE:
            enqueue_mouse_event(esp_msg->mouse_msg);
            break;
        case ESPNOW_MSG_KEYBOARD:
            enqueue_keyboard_event(esp_msg->keyboard_msg);
            break;
        case ESPNOW_MSG_GAMEPAD:
            // enqueue_gamepad_event(esp_msg->gamepad_msg);
            break;
        case ESPNOW_MSG_TEST_START:
            // enqueue_gamepad_event(esp_msg->gamepad_msg);
            send_message((uint8_t*)&packet, sizeof(packet));
            break;
        default:
            ESP_LOGI(TAG, "Unknown Format: %d", esp_msg->msg_type);
    }
}

void app_main(void){
    init_device_queues();
    begin_device_tasks();
    connect();
    init_phy();
    begin_usb_tud();
    wait_for_mount();
}