#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "wifi.h"
#include "devices.h"
#include "sleep.h"

#define HID_INTERFACE_PROTOCOL_NONE     0
#define HID_INTERFACE_PROTOCOL_KEYBOARD 1
#define HID_INTERFACE_PROTOCOL_MOUSE    2

typedef enum {
    KEYBOARD = HID_INTERFACE_PROTOCOL_KEYBOARD,
    MOUSE = HID_INTERFACE_PROTOCOL_MOUSE,
    OTHER = HID_INTERFACE_PROTOCOL_NONE
} device_type_t;

static const char* TAG = "USB_TRANSMITTER // hardware.c";

static esp_err_t process_input_report(hid_host_device_handle_t hid_device_handle, device_type_t device_type){
    uint8_t ret_val = ESP_FAIL;
    uint8_t raw_data[32];
    size_t data_length = 0;
    if (hid_host_device_get_raw_input_report_data(hid_device_handle, raw_data, sizeof(raw_data), &data_length) == ESP_OK){
        uint8_t* formatted_data = NULL;
        size_t formatted_length = 0;
        switch (device_type){
            case KEYBOARD:
                process_keyboard_report(raw_data, data_length,
                                        &formatted_data, &formatted_length);
                break;
            case MOUSE:
                process_mouse_report(raw_data, data_length,
                                        &formatted_data, &formatted_length);
                break;
            case OTHER:
            default:
                // msg = process_gamepad_report(raw_data, data_length);
                break;
        }
    
        if (formatted_data != NULL){
            ESP_LOGI(TAG, "Attempting to send message...");
            ret_val = send_message(formatted_data, formatted_length);
            free(formatted_data);
        }
    }
    return ret_val;
}

static void hid_device_interface_callback(hid_host_device_handle_t hid_device_handle, const hid_host_interface_event_t event, void* arg){
    switch (event) {
        case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
            esp_err_t err = process_input_report(hid_device_handle, (device_type_t)arg);
            if (err != ESP_OK) { ESP_LOGI(TAG, "Failed to process input-report: %s", esp_err_to_name(err)); }
            break;
        case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HID Device disconnected");
            hid_host_device_close(hid_device_handle);
            break;
        case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
            ESP_LOGW(TAG, "HID transfer error");
            break;
        default:
            ESP_LOGW(TAG, "Something went wrong with HID event. ");
            break;
    }
}

static device_type_t get_interface_type(hid_host_device_handle_t device_handle){
    hid_host_dev_params_t dev_params;
    hid_host_device_get_params(device_handle, &dev_params);
    return (device_type_t)dev_params.proto;
}

// Initliaze a connecting HID device
static void hid_host_device_callback(hid_host_device_handle_t hid_device_handle, const hid_host_driver_event_t event, void* arg){
    switch (event) {
        case HID_HOST_DRIVER_EVENT_CONNECTED:
            // Allow time for stabilization
            ESP_LOGI(TAG, "HID Device connected");

            // Configure interface callback
            const hid_host_device_config_t dev_config = {
                .callback = hid_device_interface_callback,
                .callback_arg = (void*)get_interface_type(hid_device_handle)
            };
            
            // Open and start device
            ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
            ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
            break;
        default: 
            break;
    }
}

// Event-loop to register Callbacks, Handle Enumeration, and enable the processing of HID Events 
static void usb_host_lib_task(void *arg) {
    while (true) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    }
}

// Create USB host task and any other required setup
void begin_usb_task(void){
    xTaskCreate(usb_host_lib_task, "usb_host", 8192, NULL, 5, NULL);
    begin_keyboard_watchdog();
}

// Initialize MCU D-Pins & PHY for host_mode
void init_phy(void){
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
}