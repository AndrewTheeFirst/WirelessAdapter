#pragma once
#include <stdint.h>
#include "msg_types.h"
#include "esp_err.h"

void init_device_queues(void);
void begin_device_tasks(void);

// Notify the appropriate task that USB is ready for next report
void notify_nst_task(uint8_t instance);

esp_err_t enqueue_mouse_event(espnow_msg_mouse_t mouse_msg);
esp_err_t enqueue_keyboard_event(espnow_msg_keyboard_t keyboard_msg);