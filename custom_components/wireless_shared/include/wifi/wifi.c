#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "wifi/msg_types.h"
#include "esp_err.h"
#include <string.h>
#include "wifi/wifi.h"
#include "esp_timer.h"
#include "constants.h"

#define DEBUG_WIFI DISABLED
#define UPDATE_CONN_INTERVAL_MS (4999ULL)
#define CONNECTION_TIMEOUT_US (1000000ULL)

static const char* TAG = "WIRELESS_SHARED // wifi.c";

static uint8_t peer_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
// static uint8_t peer_mac[6] = {0x10, 0x20, 0xBA, 0x4D, 0x3D, 0xCC};

void set_peer_mac(uint8_t mac[6]){
    memcpy(peer_mac, mac, 6);
}

extern void app_process_message(const espnow_message_t* msg);

static bool connection_status = false;
static esp_timer_handle_t connection_timer;

// Send message to the set peer device
// Returns esp_err_t on failure
esp_err_t send_message(const uint8_t *data, size_t size){
    return esp_now_send(peer_mac, data, size);
}

bool get_connection_status(void){
    return connection_status;
}

static void set_connection(void* arg){
    if (esp_timer_is_active(connection_timer)){
        esp_timer_stop(connection_timer);
    }
    connection_status = (bool)arg;
}

static void espnow_recv_cb(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len){
    // Drop bad message format
    #if DEBUG_WIFI
    ESP_LOGI(TAG, "A Message has been Received");
    #endif
    if (len < 1 || len > sizeof(espnow_message_t))
        return;
    switch (((espnow_message_t*)data)->msg_type){
        case ESPNOW_MSG_SYN:
            static const espnow_msg_blank_t synack = { .msg_type = ESPNOW_MSG_SYNACK };
            send_message((uint8_t*)&synack, sizeof(synack));
            break;
        case ESPNOW_MSG_SYNACK:
            static const espnow_msg_blank_t ack = { .msg_type = ESPNOW_MSG_ACK };
            send_message((uint8_t*)&ack, sizeof(ack));
            break;
        case ESPNOW_MSG_ACK:
            set_connection((void*)true);
            break;
        default:
            app_process_message((espnow_message_t*)data);
            break;
    }
}

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

// Initialize connection timer & maintain connection_status
static void connection_task(void* arg){
    const esp_timer_create_args_t timer_args = {
        .callback = set_connection,
        .arg = (void*)false
    };
    esp_timer_create(&timer_args, &connection_timer);

    while(true){
        if (esp_timer_is_active(connection_timer))
            continue;
        static const espnow_msg_blank_t syn = { .msg_type = ESPNOW_MSG_SYN };
        send_message((uint8_t*)&syn, sizeof(syn));
        esp_timer_start_once(connection_timer, CONNECTION_TIMEOUT_US);
        vTaskDelay(pdMS_TO_TICKS(UPDATE_CONN_INTERVAL_MS));
    }
}

// Initializes NVS, WIFI, ESP-NOW, and connects to peer
void connect_to_peer(void){
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

    memcpy(peer_info.peer_addr, peer_mac, 6);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));
    xTaskCreate(connection_task, "connection_task", 4096, NULL, 3, NULL);
}