#pragma once
#include "esp_err.h"
void connect(void);
esp_err_t send_message(const uint8_t *data, size_t size);