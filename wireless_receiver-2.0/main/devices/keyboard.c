#include "freertos/FreeRTOS.h"
#include "freertos/queue.h" 
#include "esp_log.h"
#include "freertos/task.h"
#include "msg_types.h"
#include "tusb_device_common.h"
#include "tusb.h"
#include "freertos/task.h"

#define KEYBOARD_QUEUE_SIZE 100

// static const char* TAG = "USB_TRANSMITTER // keyboard.c";

static QueueHandle_t keyboard_queue = NULL;

static TaskHandle_t keyboard_task_handle = NULL;

static bool __send_report(espnow_msg_keyboard_t* msg){
    return tud_hid_n_keyboard_report(
        HID_KEYBOARD_INSTANCE,
        HID_KEYBOARD_REPORT_ID,
        msg->modifiers,
        msg->keys
    );
}

static void keyboard_task(void* arg){
    espnow_msg_keyboard_t kbd_msg_buf;
    while (true){
        // attempt to retrieve msg from queue
        // restart on failure
        if (xQueueReceive(keyboard_queue, &kbd_msg_buf, portMAX_DELAY) != pdTRUE)
            continue;

        // wait for the interface to become ready
        int num_tries = 0;
        while (!tud_hid_n_ready(HID_KEYBOARD_INSTANCE) && (num_tries++ < 5)) {
            // Sleep until USB callback wakes us (or give up after 10MS)
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
            // Check if still mounted after waiting
            if (!tud_mounted()) {
                break;
            }
        }

        if (num_tries < 5) { __send_report(&kbd_msg_buf); }
    }
}

esp_err_t enqueue_keyboard_event(espnow_msg_keyboard_t keyboard_msg){
    if (xQueueSend(keyboard_queue, &keyboard_msg, 0) != pdTRUE)
        return ESP_FAIL;
    return ESP_OK; 
}

esp_err_t begin_keyboard_task(void){
    if (xTaskCreate(keyboard_task, "keyboard", 1024, NULL, 4, &keyboard_task_handle) != pdPASS)
        return ESP_FAIL;
    return ESP_OK;
}

esp_err_t init_keyboard_queue(void){
    keyboard_queue = xQueueCreate(KEYBOARD_QUEUE_SIZE, sizeof(espnow_msg_keyboard_t));
    if (keyboard_queue == 0)
        return ESP_FAIL;
    return ESP_OK;

}

void notify_keyboard_task(void){
    xTaskNotifyGive(keyboard_task_handle);
}