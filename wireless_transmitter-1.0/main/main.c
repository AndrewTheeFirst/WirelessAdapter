// OS 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

// ESP-NOW
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "msg_types.h"

// USB
#include "esp_private/usb_phy.h"
#include "usb/usb_host.h"
#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"
#include "usb/hid_usage_mouse.h"
#include "controller_usage.h"

// OTHER
#include <string.h>

#define DEBUG_HID 1
#define DEBUG_WIFI 0

#define HID_INTERFACE_PROTOCOL_NONE     0
#define HID_INTERFACE_PROTOCOL_KEYBOARD 1
#define HID_INTERFACE_PROTOCOL_MOUSE    2

#define LEN_MIN_MOUSE_REP (sizeof(hid_mouse_input_report_boot_t))
#define LEN_STD_MOUSE_REP 4
#define LEN_HIGH_PRECSICION_MOUSE_REP 6

static const char* TAG = "USB_TRANSMITTER";
static const uint8_t receiver_mac[6] = {0x10, 0x20, 0xBA, 0x4D, 0x3D, 0xCC};

static hid_host_device_handle_t keyboard_handle = NULL;
static hid_host_device_handle_t mouse_handle = NULL;
static hid_host_device_handle_t generic_handle = NULL;
controller_type_t controller_type = CONTROLLER_TYPE_UNKNOWN;

TaskHandle_t kbd_wd_task_handle = NULL;

static void process_keyboard_report(const uint8_t *data, size_t length);
static void process_mouse_report(const uint8_t *data, size_t length);
static void process_gamepad_report(const uint8_t *data, size_t length);

// ========= WIFI =========
static void espnow_send_cb(const wifi_tx_info_t* tx_info, esp_now_send_status_t status){
    #if DEBUG_WIFI
    if (status != ESP_NOW_SEND_SUCCESS){
        ESP_LOGW(TAG, "Send failed to %02X:%02X:%02X:%02X:%02X:%02X",
            tx_info->des_addr[0], tx_info->des_addr[1], tx_info->des_addr[2],
            tx_info->des_addr[3], tx_info->des_addr[4], tx_info->des_addr[5]);
    }
    #endif
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


// ========= HID Report Handling =========
typedef struct {
    bool active;
    uint32_t last_report_time;
} keyboard_watchdog_t;

static keyboard_watchdog_t kbd_wd = {0};

// Formats and sends esp-now keyboard message to receiver
// Watches for macros and enables watchdog to prevent "stuck" modifiers
static void process_keyboard_report(const uint8_t* data, size_t length) {
    // handle malformed report 
    if (length < sizeof(hid_keyboard_input_report_boot_t))
        return;

    // Convert data to standard report format
    const hid_keyboard_input_report_boot_t* report = (hid_keyboard_input_report_boot_t*)data;

    // Fill message and send
    espnow_msg_keyboard_t msg = {
        .msg_type = ESPNOW_MSG_KEYBOARD,
        .modifiers = report->modifier.val,
        .reserved = 0
    };
    memcpy(msg.keys, report->key, sizeof(report->key));
    esp_now_send(receiver_mac, (uint8_t*)&msg, sizeof(msg));

    uint32_t curr_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    // enable watchdog if modifier is sent
    if (report->modifier.val != 0){
        if (!kbd_wd.active) {
            ESP_LOGI(TAG, "kbd_wd: Watchdog Activated -> mod=0x%02X", report->modifier.val);
            kbd_wd.active = true;
            xTaskNotifyGive(kbd_wd_task_handle);
        }
    }
    // disable watchdog if report has no modifier, if enabled
    else if (kbd_wd.active) {
        ESP_LOGI(TAG, "kbd_wd: Watchdog Deactivated");
        kbd_wd.active = false;
    }
    kbd_wd.last_report_time = curr_time_ms;
}

// Task to clear "stuck" macros commonly sent by composite devices.
void keyboard_watchdog_task(void *arg) {
    espnow_msg_keyboard_t release = {
        .msg_type = ESPNOW_MSG_KEYBOARD,
        .modifiers = 0,
        .reserved = 0,
        .keys = {0, 0, 0, 0, 0, 0}
    };
    while (true) {
        // Wait until watchdog is activated
        if (!kbd_wd.active){
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(50));

        // If keyboard disconnects, disable kbd_wd and send keyboard release 
        if (keyboard_handle == NULL) {
            ESP_LOGW(TAG, "kbd_wd: Keyboard has disconnected - auto-releasing");
            kbd_wd.active = false;
            esp_now_send(receiver_mac, (uint8_t*)&release, sizeof(release));
            continue;
        }
        
        uint32_t curr_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t idle_time = curr_time_ms - kbd_wd.last_report_time;
        // fire watchdog if time between last report and macro longer than 200ms
        if (idle_time >= 200) {
            ESP_LOGW(TAG, "kbd_wd: Modifier combo timeout %dms - auto-releasing", idle_time);
            kbd_wd.active = false;
            esp_now_send(receiver_mac, (uint8_t*)&release, sizeof(release));
        }
    }
}

static inline int8_t clamp16to8(int16_t val) { return (val < -128) ? -128 : ((val > 127) ? 127 : val); }

// Formats and sends esp-now mouse message to receiver.
static void process_mouse_report(const uint8_t* data, size_t length) {
    
    if (length < LEN_MIN_MOUSE_REP)
        return;
    espnow_msg_mouse_t msg;
    if (length  <= LEN_STD_MOUSE_REP){
        const hid_mouse_input_report_boot_t* report = (hid_mouse_input_report_boot_t*)data;

        // Fill message
        msg.msg_type    = ESPNOW_MSG_MOUSE;
        msg.buttons     = report->buttons.val;
        msg.x           = report->x_displacement;
        msg.y           = report->y_displacement;
        msg.wheel       = (length == LEN_MIN_MOUSE_REP) ? 0 : data[3];
        msg.pan         = (length >  LEN_STD_MOUSE_REP) ? data[4] : 0;
    }
    else if (length >= LEN_HIGH_PRECSICION_MOUSE_REP) {
        // Bytes are little endian
        // Second byte contains signed bit.
        int16_t dx = (int16_t)((uint16_t)data[1] | ((int16_t)data[2] << 8));
        // Repeat for dy
        int16_t dy = (int16_t)((uint16_t)data[3] | ((int16_t)data[4] << 8));

        // Fill message
        msg.msg_type    = ESPNOW_MSG_MOUSE;
        msg.buttons     = data[0];
        msg.x           = clamp16to8(dx); // Clamp to 8 bits
        msg.y           = clamp16to8(dy); // Clamp to 8 bits
        msg.wheel       = data[5];
        msg.pan         = (length == LEN_HIGH_PRECSICION_MOUSE_REP) ? 0 : data[6];
    }
    // Send Message to Receiver
    esp_now_send(receiver_mac, (uint8_t*)&msg, sizeof(msg));
}

static void process_gamepad_report(const uint8_t* data, size_t length) {
    // TODO . . .
}

#if 0
#define SAITEK_LS_DX (data[1] - 0x80)
#define SAITEK_LS_DY (data[2] - 0x80)
#define SAITEK_LR_DX (data[3] - 0x80)
#define SAITEK_LR_DY (data[4] - 0x80)

static void process_gamepad_report(const uint8_t* data, size_t length) {
    espnow_msg_gamepad_t msg;
    switch(controller_type){
        case CONTROLLER_TYPE_SAITEK:
        case CONTROLLER_TYPE_SAITEK_P2500:

            const saitek_controller_report* report = (saitek_controller_report*)(data);
            msg.msg_type = ESPNOW_MSG_GAMEPAD;
            // msg.x = (int8_t)(report->lt_joystk_hor - 0x80);
            // msg.y = (int8_t)(report->lt_joystk_vert - 0x80);
            // msg.z = (int8_t)(report->rt_joystk_hor - 0x80);
            // msg.rz = (int8_t)(report->rt_joystk_vert - 0x80);
            msg.x = SAITEK_LS_DX;
            msg.y = SAITEK_LS_DY;
            msg.z = SAITEK_LR_DX;
            msg.rz = SAITEK_LR_DY;
            msg.rx = 0;
            msg.ry = 0;
            msg.hat = convert_saitek_hat(report->special & 0xF0);
            msg.buttons = convert_saitek_buttons(report->buttons, report->special);
            break;
        default:
            break;
    }
    ESP_LOGI(TAG, "Gamepad Message Sent");
    esp_now_send(receiver_mac, (uint8_t*)&msg, sizeof(msg));
}
#endif

// ========= USB SETUP =========
// Event-loop to register Callbacks, Handle Enumeration, and enable the processing of HID Events 
static void usb_host_lib_task(void *arg) {
    while (true) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    }
}

// Route HID reports and handle various HID events.
// Invoked on receiving an HID report or device disconnect
static void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle, const hid_host_interface_event_t event, void* arg){
    uint8_t data[32];
    size_t data_length = 0;
    
    switch (event) {
        case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
            // Get the input report
            ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(hid_device_handle, data, sizeof(data), &data_length));
            
            // Determine device type and process
            if (hid_device_handle == keyboard_handle){
                process_keyboard_report(data, data_length);
            }
            else if (hid_device_handle == mouse_handle) {
                process_mouse_report(data, data_length);
            }
            else if (hid_device_handle == generic_handle) {
                process_gamepad_report(data, data_length);
            }
            #if DEBUG_HID
            ESP_LOGI(TAG, "HID Device: (%s) Report Length: (%d bytes): {%02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X}", 
                (hid_device_handle == keyboard_handle) ? "Keyboard" : ((hid_device_handle == mouse_handle) ? "Mouse" : get_type_name(controller_type)),
                data_length, 
                (data_length > 0) ? data[0] : 0,
                (data_length > 1) ? data[1] : 0,
                (data_length > 2) ? data[2] : 0,
                (data_length > 3) ? data[3] : 0,
                (data_length > 4) ? data[4] : 0,
                (data_length > 5) ? data[5] : 0,
                (data_length > 6) ? data[6] : 0,
                (data_length > 7) ? data[7] : 0
            );
            #endif
            break;
            
        case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HID Device disconnected");
            if (hid_device_handle == keyboard_handle)
                keyboard_handle = NULL;
            if (hid_device_handle == mouse_handle)
                mouse_handle= NULL;
            if (hid_device_handle == generic_handle)
                generic_handle = NULL;
            hid_host_device_close(hid_device_handle);
            break;
            
        case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
            ESP_LOGW(TAG, "HID transfer error");
            break;
    }
}

static void hid_host_device_callback(hid_host_device_handle_t hid_device_handle, const hid_host_driver_event_t event, void* arg){
    switch (event) {
        case HID_HOST_DRIVER_EVENT_CONNECTED:
            // Allow time for stabilization
            ESP_LOGI(TAG, "HID Device connected");
            
            // Get device parameters to determine type
            hid_host_dev_params_t dev_params;
            esp_err_t err = hid_host_device_get_params(hid_device_handle, &dev_params);
            
            if (err == ESP_OK) {
                // Assign handle based on protocol
                switch (dev_params.proto) {
                    case HID_INTERFACE_PROTOCOL_KEYBOARD: 
                        ESP_LOGI(TAG, "Keyboard detected");
                        keyboard_handle = hid_device_handle;
                        break;
                        
                    case HID_INTERFACE_PROTOCOL_MOUSE: 
                        ESP_LOGI(TAG, "Mouse detected");
                        mouse_handle = hid_device_handle;
                        break;
                        
                    case HID_INTERFACE_PROTOCOL_NONE: 
                    default:
                        hid_host_dev_info_t dev_info;
                        if (hid_host_get_device_info(hid_device_handle, &dev_info) == ESP_OK) {
                            #if DEBUG_HID
                            ESP_LOGI(TAG, "VID:  0x%04X, PID: 0x%04X", dev_info.VID, dev_info.PID);
                            ESP_LOGI(TAG, "Manufacturer: %ls", dev_info.iManufacturer);
                            ESP_LOGI(TAG, "Product: %ls", dev_info.iProduct);
                            #endif
                            identify_controller(dev_info.VID, dev_info.PID, &controller_type);
                        }
                        generic_handle = hid_device_handle;
                        break;
                }
            }
            else {
                ESP_LOGW(TAG, "Failed to get device params: %s", esp_err_to_name(err));
                generic_handle = hid_device_handle;
            }
            
            // Configure interface callback
            const hid_host_device_config_t dev_config = {
                .callback = hid_host_interface_callback,
                .callback_arg = NULL
            };
            
            // Open and start device
            ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
            ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
            break;
        default: 
            break;
    }
}


void app_main(void){
    // Initialize USB Phy, Install Host Driver
    // Sets GPIO 19 & 20 as D+/D-
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    // Install HID host driver
    const hid_host_driver_config_t hid_host_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 8192,
        .callback = hid_host_device_callback,
        .callback_arg = NULL
    };
    ESP_ERROR_CHECK(hid_host_install(&hid_host_config));
    
    // Create USB host task
    xTaskCreate(usb_host_lib_task, "usb_host", 8192, NULL, 5, NULL);

    xTaskCreate(keyboard_watchdog_task, "kbd_watchdog", 8192, NULL, 4, NULL);

    // Initialize ESP-NOW
    init_espnow();
}