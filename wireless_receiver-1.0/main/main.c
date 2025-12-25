#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_private/usb_phy.h"
#include "tusb.h"
#include <string.h>
#include "espnow_protocol.h"

static const char *TAG = "usb_hid_bridge";
static usb_phy_handle_t phy_hdl = NULL;

// Queue for ESP-NOW messages
static QueueHandle_t espnow_queue = NULL;
#define ESPNOW_QUEUE_SIZE 20

// HID interface indices
#define HID_ITF_MOUSE    0
#define HID_ITF_KEYBOARD 1

// ============== TinyUSB Task ==============
static void tinyusb_device_task(void *arg)
{
    ESP_LOGI(TAG, "TinyUSB device task started");
    while (1) {
        tud_task();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ============== ESP-NOW Callbacks ==============
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    if (len < 1 || len > sizeof(espnow_message_t)) {
        return;
    }

    espnow_message_t *msg = (espnow_message_t *)data;
    
    // Check if USB is mounted
    if (! tud_mounted()) {
        return;  // Drop if not connected
    }
    
    // Process SIMPLE messages directly for minimum latency
    switch (msg->msg_type) {
        case ESPNOW_MSG_MOUSE_MOVE:
            // Fast path - process directly
            if (tud_hid_n_ready(HID_ITF_MOUSE)) {
                tud_hid_n_mouse_report(HID_ITF_MOUSE, 1, 
                    msg->mouse_move. buttons,
                    msg->mouse_move.dx,
                    msg->mouse_move.dy,
                    msg->mouse_move.wheel, 0);
            }
            // If not ready, drop it (mouse moves are continuous anyway)
            break;
            
        case ESPNOW_MSG_KEYBOARD_PRESS:
            // Fast path - process directly
            if (tud_hid_n_ready(HID_ITF_KEYBOARD)) {
                uint8_t keycodes[6] = {msg->keyboard_press.keycode, 0, 0, 0, 0, 0};
                tud_hid_n_keyboard_report(HID_ITF_KEYBOARD, 2, 
                    msg->keyboard_press. modifiers, keycodes);
            } else {
                // Key presses are important - queue if USB busy
                espnow_message_t msg_copy;
                memcpy(&msg_copy, msg, len);
                xQueueSend(espnow_queue, &msg_copy, 0);
            }
            break;
            
        case ESPNOW_MSG_KEYBOARD_RELEASE:
            // Fast path - process directly
            if (tud_hid_n_ready(HID_ITF_KEYBOARD)) {
                uint8_t keycodes[6] = {0, 0, 0, 0, 0, 0};
                tud_hid_n_keyboard_report(HID_ITF_KEYBOARD, 2, 0, keycodes);
            } else {
                // Queue if busy
                espnow_message_t msg_copy;
                memcpy(&msg_copy, msg, len);
                xQueueSend(espnow_queue, &msg_copy, 0);
            }
            break;
            
        case ESPNOW_MSG_MOUSE_CLICK:
        case ESPNOW_MSG_KEYBOARD_TYPE_STRING:
            // COMPLEX messages - always queue (need delays, loops, etc.)
            {
                espnow_message_t msg_copy;
                memcpy(&msg_copy, msg, len);
                xQueueSend(espnow_queue, &msg_copy, 0);
            }
            break;
            
        default:
            break;
    }
}

static void espnow_send_cb(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
{
    (void)tx_info;  // Unused in receiver
    (void)status;   // Unused in receiver
    
    // Receiver typically doesn't send, so this might never be called
    // But we need it registered anyway
}

// ============== ESP-NOW Initialization ==============
static esp_err_t init_espnow(void)
{
    ESP_LOGI(TAG, "Initializing ESP-NOW...");
    
    // Initialize NVS
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
    
    // Set long range mode (optional, better range but lower speed)
    ESP_ERROR_CHECK(esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR));
    
    // Initialize ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    
    // Print MAC address
    uint8_t mac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
    ESP_LOGI(TAG, "ESP-NOW initialized.  MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    return ESP_OK;
}

// ============== HID Action Handlers ==============
static void handle_mouse_move(const espnow_mouse_move_t *msg) __attribute__((unused));

static void handle_mouse_move(const espnow_mouse_move_t *msg)
{
    // ... implementation ...
}

static void handle_mouse_click(const espnow_mouse_click_t *msg)
{
    if (!tud_mounted() || !tud_hid_n_ready(HID_ITF_MOUSE)) {
        return;
    }
    
    // Press
    tud_hid_n_mouse_report(HID_ITF_MOUSE, 1, msg->buttons, 0, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(msg->duration_ms));
    
    // Release
    tud_hid_n_mouse_report(HID_ITF_MOUSE, 1, 0, 0, 0, 0, 0);
}

static void handle_keyboard_press(const espnow_keyboard_press_t *msg)
{
    if (!tud_mounted()) {
        return;
    }
    
    // Wait for interface to be ready with timeout
    uint32_t timeout = 10;  // 10ms max wait
    while (!tud_hid_n_ready(HID_ITF_KEYBOARD) && timeout-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    if (! tud_hid_n_ready(HID_ITF_KEYBOARD)) {
        ESP_LOGW(TAG, "Keyboard not ready, dropping press");
        return;
    }
    
    uint8_t keycodes[6] = {msg->keycode, 0, 0, 0, 0, 0};
    tud_hid_n_keyboard_report(HID_ITF_KEYBOARD, 2, msg->modifiers, keycodes);
    
    // Small delay to ensure USB transaction completes
    vTaskDelay(pdMS_TO_TICKS(2));  // 2ms safety buffer
}

static void handle_keyboard_release(const espnow_keyboard_release_t *msg)
{
    if (!tud_mounted()) {
        return;
    }
    
    uint32_t timeout = 10;
    while (!tud_hid_n_ready(HID_ITF_KEYBOARD) && timeout-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    if (!tud_hid_n_ready(HID_ITF_KEYBOARD)) {
        ESP_LOGW(TAG, "Keyboard not ready, dropping release");
        return;
    }
    
    uint8_t keycodes[6] = {0, 0, 0, 0, 0, 0};
    tud_hid_n_keyboard_report(HID_ITF_KEYBOARD, 2, 0, keycodes);
    
    vTaskDelay(pdMS_TO_TICKS(2));  // 2ms safety buffer
}

static void handle_keyboard_string(const espnow_keyboard_string_t *msg)
{
    if (!tud_mounted() || !tud_hid_n_ready(HID_ITF_KEYBOARD)) {
        return;
    }
    
    ESP_LOGI(TAG, "Typing string: %.*s", msg->length, msg->text);
    
    for (int i = 0; i < msg->length; i++) {
        char c = msg->text[i];
        uint8_t keycode = 0;
        uint8_t modifier = 0;
        
        // Simple ASCII to HID keycode conversion (expand as needed)
        if (c >= 'a' && c <= 'z') {
            keycode = HID_KEY_A + (c - 'a');
        } else if (c >= 'A' && c <= 'Z') {
            keycode = HID_KEY_A + (c - 'A');
            modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
        } else if (c == ' ') {
            keycode = HID_KEY_SPACE;
        } else if (c >= '0' && c <= '9') {
            keycode = HID_KEY_0 + (c - '0');
        }
        // Add more character mappings as needed... 
        
        if (keycode != 0) {
            uint8_t keycodes[6] = {keycode, 0, 0, 0, 0, 0};
            
            // Press
            tud_hid_n_keyboard_report(HID_ITF_KEYBOARD, 2, modifier, keycodes);
            vTaskDelay(pdMS_TO_TICKS(20));
            
            // Release
            memset(keycodes, 0, sizeof(keycodes));
            tud_hid_n_keyboard_report(HID_ITF_KEYBOARD, 2, 0, keycodes);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

// ============== ESP-NOW Message Handler Task ==============
static void espnow_handler_task(void *arg)
{
    espnow_message_t msg;
    
    ESP_LOGI(TAG, "ESP-NOW handler task started");
    
    while (1) {
        // Wait for messages that were queued
        if (xQueueReceive(espnow_queue, &msg, portMAX_DELAY) == pdTRUE) {
            
            if (! tud_mounted()) {
                continue;
            }
            
            switch (msg.msg_type) {
                case ESPNOW_MSG_MOUSE_CLICK:
                    handle_mouse_click(&msg.mouse_click);
                    break;
                    
                case ESPNOW_MSG_KEYBOARD_TYPE_STRING:
                    handle_keyboard_string(&msg.keyboard_string);
                    break;
                    
                case ESPNOW_MSG_KEYBOARD_PRESS:
                    // Queued because USB was busy - process with safety
                    handle_keyboard_press(&msg.keyboard_press);
                    break;
                    
                case ESPNOW_MSG_KEYBOARD_RELEASE:
                    // Queued because USB was busy - process with safety
                    handle_keyboard_release(&msg. keyboard_release);
                    break;
                    
                default:
                    ESP_LOGW(TAG, "Unknown queued message: 0x%02X", msg.msg_type);
                    break;
            }
        }
    }
}

// ============== Main ==============
void app_main(void)
{
    ESP_LOGI(TAG, "=== USB HID Bridge with ESP-NOW ===");
    
    // Initialize USB PHY
    ESP_LOGI(TAG, "Initializing USB PHY...");
    usb_phy_config_t phy_config = {
        .controller = USB_PHY_CTRL_OTG,
        .target = USB_PHY_TARGET_INT,
        .otg_mode = USB_OTG_MODE_DEVICE,
        .otg_speed = USB_PHY_SPEED_FULL,
        .ext_io_conf = NULL,
        .otg_io_conf = NULL,
    };
    ESP_ERROR_CHECK(usb_new_phy(&phy_config, &phy_hdl));
    
    // Initialize TinyUSB
    ESP_LOGI(TAG, "Initializing TinyUSB...");
    if (! tusb_init()) {
        ESP_LOGE(TAG, "TinyUSB init failed!");
        return;
    }
    
    // Start TinyUSB task
    xTaskCreatePinnedToCore(tinyusb_device_task, "tud_task", 4096, NULL, 5, NULL, 0);
    
    // Initialize ESP-NOW
    ESP_ERROR_CHECK(init_espnow());
    
    // Create message queue
    espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_message_t));
    if (espnow_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create ESP-NOW queue");
        return;
    }
    
    // Start ESP-NOW handler task
    xTaskCreate(espnow_handler_task, "espnow_handler", 4096, NULL, 7, NULL);
    
    // Wait for USB mount
    ESP_LOGI(TAG, "Waiting for USB to mount...");
    while (!tud_mounted()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(TAG, "USB mounted!  Ready to receive ESP-NOW commands.");
    
    // Main loop - just monitor status
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "Status: USB %s, Queue: %d messages",
                 tud_mounted() ? "connected" : "disconnected",
                 uxQueueMessagesWaiting(espnow_queue));
    }
}