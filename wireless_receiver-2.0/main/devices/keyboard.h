#pragma once
#include "wifi/msg_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern TaskHandle_t keyboard_task_handle;

esp_err_t enqueue_keyboard_event(espnow_msg_keyboard_t keyboard_msg);

esp_err_t begin_keyboard_task(void);

esp_err_t init_keyboard_queue(void);

void notify_keyboard_task(void);