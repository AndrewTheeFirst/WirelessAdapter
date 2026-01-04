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
#include "msg_types.h"
#include "tusb/usb_common.h"

#define KEYBOARD_QUEUE_SIZE 200
#define MOUSE_QUEUE_SIZE 10
#define GAMEPAD_QUEUE_SIZE 10

static const char* TAG = "USB_RECEIVER";

static usb_phy_handle_t phy_hdl = NULL;

static QueueHandle_t keyboard_queue = NULL;
static QueueHandle_t mouse_queue = NULL;
static QueueHandle_t gamepad_queue = NULL;

static TaskHandle_t keyboard_task_handle = NULL;
static TaskHandle_t mouse_task_handle = NULL;
static TaskHandle_t gamepad_task_handle = NULL;

// HID Callbacks
// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
// Leave empty, devices will send reports according to their polling rate
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen){
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
// ! ! ! TO IMPLEMENT ! ! !
// Requires: Host -> Device Communication
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
}

// Invoked when a HID report is completed
// Notifies appropriate queue that their particular HID instance will be ready soon
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
    (void)report;
    (void)len;
    
    // Notify the appropriate task that USB is ready for next report
    switch(instance) {
        case HID_KEYBOARD_INSTANCE: 
            if (keyboard_task_handle != NULL) {
                xTaskNotifyGive(keyboard_task_handle);
            }
            break;
        case HID_MOUSE_INSTANCE:
            if (mouse_task_handle != NULL) {
                xTaskNotifyGive(mouse_task_handle);
            }
            break;
        case HID_GAMEPAD_INSTANCE:
            if (gamepad_task_handle != NULL) {
                xTaskNotifyGive(gamepad_task_handle);
            }
            break;
    }
}

// ESP-NOW Callbacks
// ESP-NOW send callback -- invoked when data is sent
// Receiver currently doesn't add a peer, and does not invoke this callback
static void espnow_send_cb(const wifi_tx_info_t* tx_info, esp_now_send_status_t status){
    (void)tx_info;
    (void)status;
}

// ESP-NOW receive callback -- invoked when data is received
// Routes messages to their repective queues
static void espnow_recv_cb(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len){
    // Drop bad message format
    if (len < 1 || len > sizeof(espnow_message_t))
        return;
    // Drop if not connected
    if (!tud_mounted())
        return;
    ESP_LOGI(TAG, "A message has been received.");
    espnow_message_t* msg = (espnow_message_t*)data;
    // Route message
    switch (msg->msg_type) {
        case ESPNOW_MSG_MOUSE:
            if (xQueueSend(mouse_queue, &msg->mouse_msg, 0) != pdTRUE)
                ESP_LOGW(TAG, "Mouse queue full");
            break;
        case ESPNOW_MSG_KEYBOARD:
            if (xQueueSend(keyboard_queue, &msg->keyboard_msg, 0) != pdTRUE)
                ESP_LOGW(TAG, "Keyboard queue full");
            break;
        case ESPNOW_MSG_GAMEPAD:
            if (xQueueSend(gamepad_queue, &msg->gamepad_msg, 0) != pdTRUE)
                ESP_LOGW(TAG, "Gamepad queue full");
            break;
    }
}

// Initializes NVS, WIFI, and ESP-NOW w/ callbacks
static void init_espnow(void){
    ESP_LOGI(TAG, "Initializing ESP-NOW...");
    
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
}

// Task to process and deliver USB events to host
// tud_task() wrapper to control polling rate
static void tinyusb_device_task(void* arg){
    ESP_LOGI(TAG, "TinyUSB device task started");
    while (true) {
        tud_task();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// HID Tasks to service message queue, once notified by tud_hid_report_complete_cb()
static void keyboard_task(void* arg){
    espnow_msg_keyboard_t msg;
    ESP_LOGI(TAG, "Keyboard task started");
    while (true) {
        // Suspends task to wait for keyboard message to arrive in queue
        // returns pdTRUE if msg is retrieved from the queue successfully
        if (xQueueReceive(keyboard_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        
        // Check if USB is mounted
        if (!tud_mounted()) {
            ESP_LOGW(TAG, "USB not mounted, dropping keyboard message");
            continue;
        }
        
        // Wait for USB to be ready
        while (!tud_hid_n_ready(HID_KEYBOARD_INSTANCE)) {
            // Sleep until USB callback wakes us (or wait 100MS)
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
            
            // Check if still mounted after waiting
            if (!tud_mounted()) {
                break;
            }
        }
        
        // Send the report
        if (tud_mounted() && tud_hid_n_ready(HID_KEYBOARD_INSTANCE)){
            tud_hid_n_keyboard_report(
                HID_KEYBOARD_INSTANCE,
                HID_KEYBOARD_REPORT_ID,
                msg.modifiers,
                msg.keys
            );
        }
        else {
            ESP_LOGW(TAG, "Keyboard timeout, dropping message");
        }
    }
}

static void mouse_task(void* arg){
    espnow_msg_mouse_t msg;
    ESP_LOGI(TAG, "Mouse task started");
    while (true) {
        // Suspends task to wait for mouse message to arrive in queue
        // returns pdTRUE if msg is retrieved from the queue successfully
        if (xQueueReceive(mouse_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        
        // Check if USB is mounted
        if (!tud_mounted()) {
            ESP_LOGW(TAG, "USB not mounted, dropping mouse message");
            continue;
        }
        
        // Wait for USB to be ready
        while (!tud_hid_n_ready(HID_MOUSE_INSTANCE)) {
            // Sleep until USB callback wakes us (or wait 100MS)
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
            
            // Check if still mounted after waiting
            if (!tud_mounted()) {
                break;
            }
        }
        
        // Send the report
        if (tud_mounted() && tud_hid_n_ready(HID_MOUSE_INSTANCE)){
            tud_hid_n_mouse_report(
                HID_MOUSE_INSTANCE,
                HID_MOUSE_REPORT_ID,
                msg.buttons,
                msg.x,
                msg.y,
                msg.wheel,
                msg.pan
            );
        }
        else {
            ESP_LOGW(TAG, "Mouse timeout, dropping message");
        }
    }
}

static void gamepad_task(void* arg){
    espnow_msg_gamepad_t msg;
    ESP_LOGI(TAG, "Gamepad task started");
    while (true) {
        // Suspends task to wait for gampad message to arrive in queue
        // returns pdTRUE if msg is retrieved from the queue successfully
        if (xQueueReceive(gamepad_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        
        // Check if USB is mounted
        if (!tud_mounted()) {
            ESP_LOGW(TAG, "USB not mounted, dropping gamepad message");
            continue;
        }
        
        // Wait for USB to be ready
        while (!tud_hid_n_ready(HID_GAMEPAD_INSTANCE)) {
            // Sleep until USB callback wakes us (or wait 100MS)
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
            
            // Check if still mounted after waiting
            if (!tud_mounted()) {
                break;
            }
        }
        
        // Send the report
        if (tud_mounted() && tud_hid_n_ready(HID_GAMEPAD_INSTANCE)){
            tud_hid_n_gamepad_report(
                HID_GAMEPAD_INSTANCE,
                HID_GAMEPAD_REPORT_ID,
                msg.x,
                msg.y,
                msg.z,
                msg.rz,
                msg.rx,
                msg.ry,
                msg.hat,
                msg.buttons
            );
        }
        else {
            ESP_LOGW(TAG, "Gamepad timeout, dropping message");
        }
    }
}

// Initialize program & begins mainloop
void app_main(void){
    // Initialize USB Hardware Configuration
    ESP_LOGI(TAG, "Initializing USB PHY...");
    usb_phy_config_t phy_config = {
        .controller     = USB_PHY_CTRL_OTG,
        .target         = USB_PHY_TARGET_INT,
        .otg_mode       = USB_OTG_MODE_DEVICE,
        .otg_speed      = USB_PHY_SPEED_FULL,
        .ext_io_conf    = NULL,
        .otg_io_conf    = NULL};
    ESP_ERROR_CHECK(usb_new_phy(&phy_config, &phy_hdl));

    // Initialize & Start TUSB-Device
    ESP_LOGI(TAG, "Initializing TinyUSB...");
    if (!tusb_init()) {
        ESP_LOGE(TAG, "Failed to initialize TinyUSB.");
        return;
    }
    xTaskCreatePinnedToCore(tinyusb_device_task, "tud_task", 4096, NULL, 6, NULL, 0);

    // Initialize ESP-NOW
    init_espnow();

    ESP_LOGI(TAG, "Creating message queues...");
    keyboard_queue = xQueueCreate(KEYBOARD_QUEUE_SIZE, sizeof(espnow_msg_keyboard_t));
    mouse_queue = xQueueCreate(MOUSE_QUEUE_SIZE, sizeof(espnow_msg_mouse_t));
    gamepad_queue = xQueueCreate(GAMEPAD_QUEUE_SIZE, sizeof(espnow_msg_gamepad_t));
    
    if (!keyboard_queue || !mouse_queue || !gamepad_queue) {
        ESP_LOGE(TAG, "Failed to create queues!");
        return;
    }

    xTaskCreate(keyboard_task, "keyboard", 3072, NULL, 5, &keyboard_task_handle);
    xTaskCreate(mouse_task, "mouse", 3072,      NULL, 5, &mouse_task_handle);
    xTaskCreate(gamepad_task, "gamepad", 3072, NULL, 4, &gamepad_task_handle);

    // Wait for USB mount
    ESP_LOGI(TAG, "Waiting for USB to mount...");
    while (!tud_mounted()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(TAG, "USB mounted! Device is ready.");
}