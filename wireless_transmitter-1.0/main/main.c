#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_private/usb_phy.h"
#include "tusb.h"
#include "msg_types.h"

#define BUTTON_1_GPIO 17
#define BUTTON_2_GPIO 6

static const char* TAG = "USB_TRANSMITTER";
// static usb_phy_handle_t phy_hdl = NULL;
static uint8_t receiver_mac[6] = {0x10, 0x20, 0xBA, 0x4D, 0x3D, 0xCC};

static void espnow_send_cb(const wifi_tx_info_t* tx_info, esp_now_send_status_t status){
    if (status != ESP_NOW_SEND_SUCCESS){
        ESP_LOGW(TAG, "Send failed to %02X:%02X:%02X:%02X:%02X:%02X",
            tx_info->des_addr[0], tx_info->des_addr[1], tx_info->des_addr[2],
            tx_info->des_addr[3], tx_info->des_addr[4], tx_info->des_addr[5]);
    }
}

// Receiver currently doesn't receive, but ESP-NOW wants CB registered.
static void espnow_recv_cb(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len){
    (void)recv_info;
    (void)data;
    (void)len;
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

    // Add receiver as peer
    esp_now_peer_info_t peer_info = {
        .channel = 0,
        .ifidx = ESP_IF_WIFI_STA,
        .encrypt = false,
    };
    memcpy(peer_info.peer_addr, receiver_mac, 6);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));
}

static void tinyusb_host_task(void* arg){
    ESP_LOGI(TAG, "TinyUSB Host task started");
    while (true) {
        tuh_task();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void send_keyboard_msg(uint8_t modifiers, uint8_t keys[6]){
    espnow_msg_keyboard_t msg = {
        .msg_type = ESPNOW_MSG_KEYBOARD,
        .modifiers = modifiers,
        .reserved = 0,
        .keys = {keys[0], keys[1], keys[2], keys[3], keys[4], keys[5]}
    };
    esp_now_send(receiver_mac, (uint8_t*)&msg, sizeof(msg));
}

static void send_mouse_msg(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel, int8_t pan){
    espnow_msg_mouse_t msg = {
        .msg_type = ESPNOW_MSG_MOUSE,
        .buttons = buttons,
        .x = dx,
        .y = dy,
        .wheel = wheel,
        .pan = pan,
    };
    esp_now_send(receiver_mac, (uint8_t*)&msg, sizeof(msg));
}

static void send_gamepad_msg(int8_t ldx, int8_t ldy, int8_t rdx, int8_t rdy, int8_t ltrig, int8_t rtrig, uint8_t dpad, uint8_t buttons){
    espnow_msg_gamepad_t msg = {
        .msg_type = ESPNOW_MSG_GAMEPAD,
        .x = ldx,
        .y = ldy,
        .z = rdx,
        .rz = rdy,
        .rx = ltrig,
        .ry = rtrig,
        .hat = dpad,
        .buttons = buttons
    };
    esp_now_send(receiver_mac, (uint8_t*)&msg, sizeof(msg));
}

static void send_keyboard_msg_string(const char* message){
    uint8_t const conv_table[128][2] =  { HID_ASCII_TO_KEYCODE };
    for (int index = 0; index < strlen(message); index++){
        uint8_t chr = (uint8_t)message[index];
        uint8_t modifier = 0;
        uint8_t keycode[6] = {0};
        if (conv_table[chr][0])
            modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
        keycode[0] = conv_table[chr][1];
        send_keyboard_msg(modifier, keycode);
        vTaskDelay(pdMS_TO_TICKS(10));
        uint8_t clear[6] = {0, 0, 0, 0, 0, 0};
        send_keyboard_msg(0, clear);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void init_buttons(void){
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

static void button_task(void* arg){
    uint8_t KEY_A[6] = {HID_KEY_A, 0, 0, 0, 0, 0};
    uint8_t CLEAR[6] = {0, 0, 0, 0, 0, 0};
    
    init_buttons();

    // Track button states for edge detection
    bool button1_prev = false;  // true = not pressed (pull-up)
    bool button2_prev = false;

    while (true){
        // Read current button states (LOW = pressed with pull-up)
        bool button1_current = gpio_get_level(BUTTON_1_GPIO);
        bool button2_current = gpio_get_level(BUTTON_2_GPIO);
        
        // Button 1 - Send 'A' key
        if (button1_current && !button1_prev){
            // Button pressed (falling edge)
            ESP_LOGI(TAG, "Button 1 pressed - Sending 'A'");
            send_keyboard_msg(0, KEY_A);
        }
        else if (!button1_current && button1_prev){
            // Button released (rising edge)
            ESP_LOGI(TAG, "Button 1 released");
            send_keyboard_msg(0, CLEAR);
        }
        
        // Button 2 - Send Message
        if (button2_current && !button2_prev){
            // Button pressed (falling edge)
            ESP_LOGI(TAG, "Button 2 pressed - Sending Secret Message");
            send_keyboard_msg_string("This is a message to test the speed capabilities of the receiver.\nThis message is being sent at approximatly 1200wpm.");
        } 
        else if (!button2_current && button2_prev){
            // Button released (rising edge)
            ESP_LOGI(TAG, "Button 2 released");
            send_keyboard_msg(0, CLEAR);
        }
        
        // Update previous states
        button1_prev = button1_current;
        button2_prev = button2_current;
        
        // Poll every 10ms (100Hz)
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void){
    // // Initialize USB Hardware Configuration
    // ESP_LOGI(TAG, "Initializing USB PHY...");
    // usb_phy_config_t phy_config = {
    //     .controller = USB_PHY_CTRL_OTG,
    //     .target = USB_PHY_TARGET_INT,
    //     .otg_mode = USB_OTG_MODE_HOST,
    //     .otg_speed = USB_PHY_SPEED_FULL,
    //     .ext_io_conf = NULL,
    //     .otg_io_conf = NULL};
    // ESP_ERROR_CHECK(usb_new_phy(&phy_config, &phy_hdl));

    //     // Initialize & Start TUSB-Host
    // ESP_LOGI(TAG, "Initializing TinyUSB...");
    // if (!tusb_init()) {
    //     ESP_LOGE(TAG, "Failed to initialize TinyUSB.");
    //     return;
    // }
    // xTaskCreatePinnedToCore(tinyusb_host_task, "tuh_task", 4096, NULL, 5, NULL, 0);

    // Initialize ESP-NOW
    init_espnow();
    
    // Start button task
    xTaskCreate(button_task, "buttons", 4096, NULL, 5, NULL);
}