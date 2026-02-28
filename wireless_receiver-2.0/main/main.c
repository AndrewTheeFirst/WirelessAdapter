#include <stdio.h>
#include "wifi/msg_types.h"
#include "wifi/wifi.h"
#include "devices.h"
#include "esp_log.h"
#include "tusb.h"
#include "hardware.h"

static const char* TAG = "USB_RECEIVER // main.c";

// message callback to be invoked when data is received -- referenced in wifi.c
// Routes messages to their repective queues
void app_process_message(const espnow_message_t* esp_msg){
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

void __print_connection_status(void* arg){
    while(true){
        ESP_LOGI(TAG, "Connection Status: %s", get_connection_status() ? "Connected" : "Not Connected");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void print_connection_status(void){
    xTaskCreate(__print_connection_status, "print_connection_status", 4096, NULL, 4, NULL);
}

void app_main(void){
    init_device_queues();
    begin_device_tasks();
    set_peer_mac((uint8_t[]){0x80, 0xB5, 0x4E, 0xDE, 0x45, 0x08});
    connect_to_peer();
    init_phy();
    ESP_ERROR_CHECK(begin_usb_tud());
    print_connection_status();
    wait_for_mount();
}