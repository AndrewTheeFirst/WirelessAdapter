#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi.h"
#include "msg_types.h"
#include "hardware.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char* TAG = "USB_TRANSMITTER // main.c";

int64_t time_start, time_end;

espnow_msg_test_t packet = {
    .msg_type = ESPNOW_MSG_TEST_START,
    .buffer = {0}
};

void benchmark_task(void* arg){
    while(true){
        if (send_message((uint8_t*)&packet, sizeof(packet)) == ESP_OK){
            ESP_LOGI(TAG, "Benchmark has begun.");
            time_start = esp_timer_get_time();
        }
        vTaskDelay(pdMS_TO_TICKS(10000UL));
    }
}

void app_process_message(const uint8_t* msg){
    time_end = esp_timer_get_time();

    switch(((espnow_message_t*)msg)->msg_type){
        case ESPNOW_MSG_TEST_END:
            ESP_LOGI(TAG, "RRT: %lld US", (time_end - time_start));
            ESP_LOGI(TAG, "Benchmark is complete.");
    }
}


void app_main(void){
    init_phy();
    connect();
    begin_usbh_task();
    xTaskCreate(benchmark_task, "benchmark_task", 1024, NULL, 5, NULL);
}

