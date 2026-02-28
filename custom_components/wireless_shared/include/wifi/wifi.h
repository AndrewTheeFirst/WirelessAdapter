#pragma once
#include "esp_err.h"
#include <stdbool.h>

void set_peer_mac(uint8_t mac[6]);
bool get_connection_status(void);
esp_err_t send_message(const uint8_t *data, size_t size);
void connect_to_peer(void);
