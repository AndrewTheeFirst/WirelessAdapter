#pragma once
#include "msg_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern TaskHandle_t mouse_task_handle;

esp_err_t enqueue_mouse_event(espnow_msg_mouse_t mouse_msg);

esp_err_t begin_mouse_task(void);

esp_err_t init_mouse_queue(void);

void notify_mouse_task(void);