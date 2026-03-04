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
#define PEER_MAC_STORAGE_KEY "peer_mac"

static const char* TAG = "WIRELESS_SHARED // wifi.c";

static uint8_t peer_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

extern void process_message_cb(const espnow_message_t* msg);
extern void connection_status_cb(bool connection_status);
extern void paired_status_updated_cb(bool paired_status);

static tristate_bool_t paired_status = TRISTATE_UNINIT;
static tristate_bool_t connection_status = TRISTATE_UNINIT;
static esp_timer_handle_t connection_timer = NULL;

static void set_connection_status(tristate_bool_t status){
    if (connection_timer && esp_timer_is_active(connection_timer))
        esp_timer_stop(connection_timer);
    if (connection_status != status){
        connection_status = status;
        connection_status_cb(connection_status == TRISTATE_TRUE);
    }
}

static inline bool is_recognized_sender(uint8_t sender_mac[6]){ return (memcmp(sender_mac, peer_mac, 6) == 0); }

// Send message to the set peer device
// Returns esp_err_t on failure
esp_err_t send_message(const uint8_t *data, size_t size){
    return esp_now_send(peer_mac, data, size);
}

static void espnow_recv_cb(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len){
    // Drop bad message format
    #if DEBUG_WIFI
    ESP_LOGI(TAG, "A Message has been Received");
    #endif
    // Ignore malformed report
    if (len < 1 || len > sizeof(espnow_message_t))
        return;
    if (paired_status == TRISTATE_TRUE && is_recognized_sender(recv_info->src_addr)){
        switch (((espnow_message_t*)data)->msg_type){
            case ESPNOW_MSG_SYN:
                static const espnow_msg_blank_t synack = { .msg_type = ESPNOW_MSG_SYNACK };
                send_message((uint8_t*)&synack, sizeof(synack));
                break;
            case ESPNOW_MSG_SYNACK:
                static const espnow_msg_blank_t ack = { .msg_type = ESPNOW_MSG_ACK };
                if (send_message((uint8_t*)&ack, sizeof(ack)) == ESP_OK)
                    set_connection_status(TRISTATE_TRUE);
                break;
            case ESPNOW_MSG_ACK:
                break;
            default:
                process_message_cb((espnow_message_t*)data);
                break;
        }
    }
    else if (paired_status == TRISTATE_FALSE){
        switch (((espnow_message_t*)data)->msg_type){
            case ESPNOW_MSG_PAIR_REQUEST:
                static const espnow_msg_blank_t pair_request = { .msg_type = ESPNOW_MSG_PAIR_REQUEST };
                if (send_message((uint8_t*)&pair_request, sizeof(pair_request)) == ESP_OK)
                break;
        }
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

static void connection_timer_cb(void* arg){
    set_connection_status(TRISTATE_FALSE);
}

// Keep connection_status up-to-date
static void connection_task(void* arg){
    const esp_timer_create_args_t timer_args = {
        .callback = connection_timer_cb,
        .arg = NULL
    };
    esp_timer_create(&timer_args, &connection_timer);

    while(true){
        vTaskDelay(pdMS_TO_TICKS(UPDATE_CONN_INTERVAL_MS));
        // wait for timer to finish before checking connection
        if (esp_timer_is_active(connection_timer))
            continue;
        // No point in checking connection if device is not yet paired
        if (paired_status != TRISTATE_TRUE)
            continue;
        // begin handshake
        static const espnow_msg_blank_t syn = { .msg_type = ESPNOW_MSG_SYN };
        send_message((uint8_t*)&syn, sizeof(syn));
        esp_timer_start_once(connection_timer, CONNECTION_TIMEOUT_US);
    }
}

// Initialize connection timer & maintain connection_status
static void begin_connection_task(void){
    xTaskCreate(connection_task, "connection_task", 4096, NULL, 3, NULL);
}

static void store_peer_mac(uint8_t mac[6]){
    nvs_handle_t nvs_handle;
    nvs_open("storage", NVS_READWRITE, &nvs_handle);
    nvs_set_blob(nvs_handle, PEER_MAC_STORAGE_KEY, mac, 6);
    nvs_close(nvs_handle);
}
// Initialize non-volitile storage for ESPNOW and Saved Config
static esp_err_t start_nvs(void){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

void set_paired_status(tristate_bool_t status){
    if (paired_status != status){
        paired_status = status;
        paired_status_updated_cb(paired_status == TRISTATE_TRUE);
    }
}

void register_peer(uint8_t mac[6]){
    esp_now_peer_info_t peer_info = {
        .channel = 0,
        .ifidx = ESP_IF_WIFI_STA,
        .encrypt = false
    };
    memcpy(peer_info.peer_addr, mac, 6);
    memcpy(peer_mac, mac, 6);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));
}

// Registers mac address saved in NVS as a peer
// If no address is found, the broadcast address will be registered instead 
static void set_peer_if_exists(void){
    nvs_handle_t nvs_handle;
    if (nvs_open("storage", NVS_READONLY, &nvs_handle) == ESP_OK){
        uint8_t saved_peer_mac[6];
        size_t data_len;
        if (nvs_get_blob(nvs_handle, PEER_MAC_STORAGE_KEY, saved_peer_mac, &data_len)){
            register_peer(saved_peer_mac);
            set_paired_status(TRISTATE_TRUE);
        }
    }
    if (paired_status != TRISTATE_TRUE){
        register_peer(peer_mac);
        set_paired_status(TRISTATE_FALSE);
    }
    if (nvs_handle)
        nvs_close(nvs_handle);
}

static void pairing_task(void* arg){
    espnow_msg_blank_t pair_request = { .msg_type = ESPNOW_MSG_PAIR_REQUEST };
    while (!paired_status){
        vTaskDelay(pdMS_TO_TICKS(5000));
        send_message((uint8_t*)&pair_request, sizeof(pair_request));
    }
    vTaskDelete(NULL);
}

void begin_pairing_task(void){
    xTaskCreate(pairing_task, "pairing task", 4096, NULL, 3, NULL);
}

// Initializes NVS, WIFI, ESP-NOW, and connects to peer
void start_espnow(void){
    // NVS preferred by ESP_NOW
    ESP_ERROR_CHECK(start_nvs());
    
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
    set_peer_if_exists();
    begin_connection_task();
}

