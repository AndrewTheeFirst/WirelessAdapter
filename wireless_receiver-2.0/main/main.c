#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi/msg_types.h"
#include "wifi/wifi.h"
#include "devices.h"
#include "esp_log.h"
#include "hardware.h"

static const char* TAG = "USB_RECEIVER // main.c";

// message callback to be invoked when data is received -- referenced in wifi.c
// Routes messages to their repective queues
void process_message_cb(const espnow_message_t* esp_msg){
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
        case ESPNOW_MSG_START_RTT:
            static const espnow_msg_blank_t packet = { .msg_type = ESPNOW_MSG_END_RTT };
            send_message((uint8_t*)&packet, sizeof(packet));
            // enqueue_gamepad_event(esp_msg->gamepad_msg);
            break;
        default:
            ESP_LOGI(TAG, "Unknown Format: %d", esp_msg->msg_type);
    }
}

void connection_status_cb(bool connection_status){
    ESP_LOGI(TAG, "Connection: %s", connection_status ? "CONNECTED" : "DISCONNECTED");
}
void paired_status_updated_cb(bool paired_status){
    ESP_LOGI(TAG, "Paired: %s", paired_status ? "YES" : "NO");
}

void app_main(void){
    init_device_queues();
    begin_device_tasks();
    start_espnow();
    unpair();
    begin_pairing_task();
    // set_new_peer((uint8_t[]){0x80, 0xB5, 0x4E, 0xDE, 0x45, 0x08});
    init_phy();
    ESP_ERROR_CHECK(begin_usb_tud());
    wait_for_mount();
}