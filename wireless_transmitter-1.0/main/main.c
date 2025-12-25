#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "espnow_protocol.h"
#include "hid.h" // no need to add tusb dependency


#define USE_DEMO_MODE 0
#define BUTTON_1_GPIO  17
#define BUTTON_2_GPIO  6

static const char *TAG = "hid_transmitter";

// ========== UPDATE WITH RECEIVER'S MAC ==========
static uint8_t receiver_mac[6] = {0x10, 0x20, 0xBA, 0x4D, 0x3D, 0xCC};

// ESP-NOW callbacks
static void espnow_send_cb(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGW(TAG, "Send failed to %02X:%02X:%02X:%02X:%02X:%02X",
                 tx_info->des_addr[0], tx_info->des_addr[1], tx_info->des_addr[2],
                 tx_info->des_addr[3], tx_info->des_addr[4], tx_info->des_addr[5]);
    }
}

// Initialize ESP-NOW
static esp_err_t init_espnow(void)
{
    ESP_LOGI(TAG, "Initializing ESP-NOW.. .");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize WiFi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Initialize ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    
    // Add receiver as peer
    esp_now_peer_info_t peer_info = {
        .channel = 0,
        .ifidx = ESP_IF_WIFI_STA,
        .encrypt = false,
    };
    memcpy(peer_info.peer_addr, receiver_mac, 6);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));
    
    // Print our MAC address
    uint8_t mac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
    ESP_LOGI(TAG, "Transmitter MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Receiver MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             receiver_mac[0], receiver_mac[1], receiver_mac[2],
             receiver_mac[3], receiver_mac[4], receiver_mac[5]);
    
    return ESP_OK;
}

// Helper functions to send messages
static void send_keyboard_press(uint8_t modifiers, uint8_t keycode)
{
    espnow_keyboard_press_t msg = {
        .msg_type = ESPNOW_MSG_KEYBOARD_PRESS,
        .modifiers = modifiers,
        .keycode = keycode
    };
    esp_now_send(receiver_mac, (uint8_t*)&msg, sizeof(msg));
}

static void send_keyboard_release(uint8_t keycode)
{
    espnow_keyboard_release_t msg = {
        .msg_type = ESPNOW_MSG_KEYBOARD_RELEASE,
        . keycode = keycode
    };
    esp_now_send(receiver_mac, (uint8_t*)&msg, sizeof(msg));
}

static void send_mouse_move(int8_t dx, int8_t dy)
{
    espnow_mouse_move_t msg = {
        .msg_type = ESPNOW_MSG_MOUSE_MOVE,
        .dx = dx,
        .dy = dy,
        .wheel = 0,
        .buttons = 0
    };
    esp_now_send(receiver_mac, (uint8_t*)&msg, sizeof(msg));
}

static void send_keyboard_string(const char *text)
{
    espnow_keyboard_string_t msg = {
        .msg_type = ESPNOW_MSG_KEYBOARD_TYPE_STRING,
        .length = strlen(text)
    };
    strncpy(msg.text, text, sizeof(msg.text) - 1);
    esp_now_send(receiver_mac, (uint8_t*)&msg, sizeof(msg));
}

#if USE_DEMO_MODE
static void demo_task(void *arg)
{
    ESP_LOGI(TAG, "=== DEMO MODE - Sending test inputs ===");
    ESP_LOGI(TAG, "Starting demo in 3 seconds...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    while (1) {
        ESP_LOGI(TAG, "Sending keyboard test...");
        
        // Type 'A'
        send_keyboard_press(0, HID_KEY_A);
        vTaskDelay(pdMS_TO_TICKS(50));
        send_keyboard_release(HID_KEY_A);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        ESP_LOGI(TAG, "Sending mouse test...");
        
        // Move mouse right
        for (int i = 0; i < 20; i++) {
            send_mouse_move(5, 0);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        ESP_LOGI(TAG, "Sending string test...");
        type_string_slowly("Hello from ESP32");
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

#else
static void init_buttons(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = ((1ULL << BUTTON_1_GPIO) | (1ULL << BUTTON_2_GPIO)),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    ESP_LOGI(TAG, "Buttons initialized on GPIO %d and %d", BUTTON_1_GPIO, BUTTON_2_GPIO);
}

static void button_task(void *arg)
{
    ESP_LOGI(TAG, "=== BUTTON MODE - Reading GPIO inputs ===");
    
    init_buttons();
    
    // Track button states for edge detection
    bool button1_prev = false;  // true = not pressed (pull-up)
    bool button2_prev = false;
    
    while (1) {
        // Read current button states (LOW = pressed with pull-up)
        bool button1_current = gpio_get_level(BUTTON_1_GPIO);
        bool button2_current = gpio_get_level(BUTTON_2_GPIO);
        
        // Button 1 - Send 'A' key
        if (button1_current && !button1_prev) {
            // Button pressed (falling edge)
            ESP_LOGI(TAG, "Button 1 pressed - Sending 'A'");
            send_keyboard_press(0, HID_KEY_A);
        } else if (!button1_current && button1_prev) {
            // Button released (rising edge)
            ESP_LOGI(TAG, "Button 1 released");
            send_keyboard_release(HID_KEY_A);
        }
        
        // Button 2 - Send 'B' key
        if (button2_current && !button2_prev) {
            // Button pressed (falling edge)
            ESP_LOGI(TAG, "Button 2 pressed - Sending 'B'");
            send_keyboard_press(0, HID_KEY_B);
        } else if (!button2_current && button2_prev) {
            // Button released (rising edge)
            ESP_LOGI(TAG, "Button 2 released");
            send_keyboard_release(HID_KEY_B);
        }
        
        // Update previous states
        button1_prev = button1_current;
        button2_prev = button2_current;
        
        // Poll every 10ms (100Hz)
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

#endif

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP-NOW HID Transmitter ===");
    
    ESP_ERROR_CHECK(init_espnow());
    
    ESP_LOGI(TAG, "Initialization complete!");
    
#if USE_DEMO_MODE
    // Start demo task
    xTaskCreate(demo_task, "demo", 4096, NULL, 5, NULL);
#else
    // Start button task
    xTaskCreate(button_task, "buttons", 4096, NULL, 5, NULL);
#endif
}