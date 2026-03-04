#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include "constants.h"

esp_err_t send_message(const uint8_t *data, size_t size);
void start_espnow(void);
void begin_pairing_task(void);
void register_peer(uint8_t mac[6]);
void set_paired_status(tristate_bool_t status);
void set_new_peer(uint8_t mac[6]);
