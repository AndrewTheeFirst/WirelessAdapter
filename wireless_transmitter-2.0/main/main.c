#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi/wifi.h"
#include "wifi/msg_types.h"
#include "hardware.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "device_config.h"
#include "driver/gpio.h"

#define LED_PIN GPIO_NUM_15

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

void process_message_cb(const espnow_message_t* msg){
#if BENCHMARK
    time_end = esp_timer_get_time();
#endif
    switch(msg->msg_type){
#if BENCHMARK
        case ESPNOW_MSG_END_RTT:
            ESP_LOGI(TAG, "RRT: %lld US", (time_end - time_start));
            break;
#endif
        default:
            break;
    }
}

TaskHandle_t blink_task_handle = NULL;

void blink_task(void* arg){
    (void)arg;
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    while (true){
        gpio_set_level(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_set_level(LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(200)); 
    }
}

void begin_blink_task(void){
    xTaskCreate(blink_task, "blink task", 2048, NULL, 3, &blink_task_handle);
}

void end_blink_task(void){
    if (blink_task_handle){
        vTaskDelete(blink_task_handle);
        blink_task_handle = NULL;
        gpio_set_level(LED_PIN, 0);
    }
}

void connection_status_cb(bool connection_status){
    ESP_LOGI(TAG, "Connection: %s", connection_status ? "CONNECTED" : "DISCONNECTED");
}

void paired_status_updated_cb(bool paired_status){
    ESP_LOGI(TAG, "Paired: %s", paired_status ? "YES" : "NO");
    if (!paired_status){
        begin_pairing_task();
        begin_blink_task();
    }
    else
        end_blink_task();
}

void app_main(void){
    init_phy();
    start_espnow();
    unpair();
    begin_usbh_task();
#if BENCHMARK
    xTaskCreate(benchmark_task, "benchmark_task", 4096, NULL, 4, NULL);
#endif
}

