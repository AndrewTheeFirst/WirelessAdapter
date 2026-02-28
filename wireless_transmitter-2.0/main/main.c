#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi/wifi.h"
#include "wifi/msg_types.h"
#include "hardware.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "device_config.h"

static const char* TAG = "USB_TRANSMITTER // main.c";

#if BENCHMARK
int64_t time_start, time_end;

void benchmark_task(void* arg){
    static espnow_msg_blank_t packet = { .msg_type = ESPNOW_MSG_START_RTT };
    while(true){
        if (send_message((uint8_t*)&packet, sizeof(packet)) == ESP_OK){
            time_start = esp_timer_get_time();
        }
        vTaskDelay(pdMS_TO_TICKS(15000UL));
    }
}
#endif

void app_process_message(const uint8_t* msg){
#if BENCHMARK
    time_end = esp_timer_get_time();
#endif
    switch(((espnow_message_t*)msg)->msg_type){
#if BENCHMARK
        case ESPNOW_MSG_END_RTT:
            ESP_LOGI(TAG, "RRT: %lld US", (time_end - time_start));
            break;
#endif
        default:
            break;
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
    init_phy();
    set_peer_mac((uint8_t[]){0x10, 0x20, 0xBA, 0x4D, 0x3D, 0xCC});
    connect_to_peer();
    begin_usbh_task();
    print_connection_status();
#if BENCHMARK
    xTaskCreate(benchmark_task, "benchmark_task", 4096, NULL, 4, NULL);
#endif
}
