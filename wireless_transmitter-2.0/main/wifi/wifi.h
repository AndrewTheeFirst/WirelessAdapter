#include <stdint.h>
#include "esp_err.h"
#include <stddef.h>
// Initializes NVS, WIFI, and ESP-NOW w/ callbacks
void connect(void);
esp_err_t send_message(const uint8_t *data, size_t size);