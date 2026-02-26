#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "msg_types.h"
#include "esp_err.h"
#include <string.h>
#include "wifi.h"
#include "device_config.h"

static const char* TAG = "USB_RECEIVER // wifi.c";

static const uint8_t transmitter_peer_mac[6] = {0x80, 0xB5, 0x4E, 0xDE, 0x45, 0x08};

extern void app_process_message(const uint8_t* msg);

// static bool connection_status = false;

// ESP-NOW Callbacks
// ESP-NOW send callback -- invoked when data is sent
static void espnow_send_cb(const wifi_tx_info_t* tx_info, esp_now_send_status_t status){
    #if (DEBUG_WIFI)
        if (status == (esp_now_send_status_t)WIFI_SEND_SUCCESS) ESP_LOGI(TAG, "Message Sent Successfully");
        else ESP_LOGI(TAG, "Message Failed to Send");
    #else
        (void)status;
    #endif
    (void)tx_info;
    
}

// Initializes NVS, WIFI, ESP-NOW, and connects to peer
static void espnow_recv_cb(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len){
    // Drop bad message format
    #if DEBUG_WIFI
        ESP_LOGI(TAG, "A Message has been Received");
    #endif
    if (len < 1 || len > sizeof(espnow_message_t))
        return;
    app_process_message(data);
}

// Initializes NVS, WIFI, ESP-NOW, and connects to peer
void connect(void){
    // Initialize NVS (not used, but satisfies ESP-NOW)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize WiFi (required for ESP-NOW)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Set long range mode (better range at the cost of speed)
    ESP_ERROR_CHECK(esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR));
    
    // Initialize ESP-NOW & Register Callbacks
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));

    // Add receiver as peer
    esp_now_peer_info_t peer_info = {
        .channel = 0,
        .ifidx = ESP_IF_WIFI_STA,
        .encrypt = false,
    };
    memcpy(peer_info.peer_addr, transmitter_peer_mac, 6);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));
}

// Send message to the set peer device
// Returns esp_err_t on failure
esp_err_t send_message(const uint8_t *data, size_t size){
    return esp_now_send(transmitter_peer_mac, data, size);
} 