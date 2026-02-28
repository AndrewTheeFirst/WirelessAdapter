#include "freertos/FreeRTOS.h"
#include "freertos/queue.h" 
#include "esp_log.h"
#include "freertos/task.h"
#include "wifi/msg_types.h"
#include "tusb_device_common.h"
#include "tusb.h"

#define MOUSE_QUEUE_SIZE 100

// static const char* TAG = "USB_TRANSMITTER // mouse.c";

static QueueHandle_t mouse_queue = NULL;

static TaskHandle_t mouse_task_handle = NULL;

static bool __send_report(espnow_msg_mouse_t* msg){
    return tud_hid_n_mouse_report(
        HID_MOUSE_INSTANCE,
        HID_MOUSE_REPORT_ID,
        msg->buttons,
        msg->x,
        msg->y,
        msg->wheel,
        msg->pan
    );
}

static void mouse_task(void* arg){
    espnow_msg_mouse_t mouse_msg_buf;
    while (true){
        // attempt to retrieve msg from queue
        // restart on failure
        if (xQueueReceive(mouse_queue, &mouse_msg_buf, portMAX_DELAY) != pdTRUE)
            continue;

        // wait for the interface to become ready
        int num_tries = 0;
        while (!tud_hid_n_ready(HID_MOUSE_INSTANCE) && (num_tries++ < 5)) {
            // Sleep until USB callback wakes us (or give up after 10MS)
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
            // Check if still mounted after waiting
            if (!tud_mounted()) {
                break;
            }
        }

        if (num_tries < 5) { __send_report(&mouse_msg_buf); }
    }
}

esp_err_t enqueue_mouse_event(espnow_msg_mouse_t mouse_msg){
    if (xQueueSend(mouse_queue, &mouse_msg, 0) != pdTRUE)
        return ESP_FAIL;
    return ESP_OK;
}

esp_err_t begin_mouse_task(void){
    if (xTaskCreate(mouse_task, "mouse", 1024, NULL, 4, &mouse_task_handle) != pdPASS)
        return ESP_FAIL;
    return ESP_OK;
}

esp_err_t init_mouse_queue(void){
    mouse_queue = xQueueCreate(MOUSE_QUEUE_SIZE, sizeof(espnow_msg_mouse_t));
    if (mouse_queue == 0)
        return ESP_FAIL;
    return ESP_OK;
}

void notify_mouse_task(void){
    xTaskNotifyGive(mouse_task_handle);
}