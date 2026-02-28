#include "wifi/msg_types.h"
#include "usb/hid_usage_keyboard.h"
#include "esp_task.h"
#include "esp_log.h"
#include "constants.h"
#include "wifi/wifi.h"
#include <string.h>

static const char* TAG = "USB_TRANSMITTER // keyboard.c";

typedef struct kwt {
    bool active;
    uint32_t last_report_time;
} keyboard_watchdog_t;

static keyboard_watchdog_t kbd_wd = {0};

static TaskHandle_t kbd_wd_task_handle = NULL;

static inline bool is_modifier(uint8_t mask){ return mask != 0; }

// update keyboard watchdog timer given modifier mask
static void update_kbd_wd(uint8_t mask){
    uint32_t curr_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    // enable watchdog if modifier is sent
    if (is_modifier(mask)){
        if (!kbd_wd.active) {
            ESP_LOGI(TAG, "kbd_wd: Watchdog Activated -> mod=0x%02X", mask);
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
static void keyboard_watchdog_task(void* arg) {
    espnow_msg_keyboard_t release = {
        .msg_type = ESPNOW_MSG_KEYBOARD,
        .modifiers = 0,
        .reserved = 0,
        .keys = {0}
    };
    while (true) {
        // Wait until watchdog is activated
        if (!kbd_wd.active){
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
        
        uint32_t curr_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t idle_time = curr_time_ms - kbd_wd.last_report_time;
        // fire watchdog if time between last report and macro longer than 200ms
        if (idle_time >= 200) {
            ESP_LOGW(TAG, "kbd_wd: Modifier combo timeout %dms - auto-releasing", idle_time);
            kbd_wd.active = false;
            send_message((uint8_t*)&release, sizeof(release));
        }
    }
}

void begin_keyboard_watchdog(void){
    xTaskCreate(keyboard_watchdog_task, "kbd_watchdog", 8192, NULL, 4, NULL);
}

// parse a keyboard input-report and return a formatted espnow_message
esp_err_t process_keyboard_report(const uint8_t* data, size_t length, uint8_t** formatted_data, size_t* formatted_length) {
    // handle malformed report 
    if (length < sizeof(hid_keyboard_input_report_boot_t))
        return ESP_FAIL;

    // Convert data to standard report format
    const hid_keyboard_input_report_boot_t* report = (hid_keyboard_input_report_boot_t*)data;

    // Fill message and send
    espnow_msg_keyboard_t* msg = (espnow_msg_keyboard_t*)malloc(sizeof(espnow_msg_keyboard_t));
    msg->msg_type = ESPNOW_MSG_KEYBOARD;
    memcpy(msg->keys, report->key, sizeof(report->key));
    msg->modifiers = report->modifier.val;
    msg->reserved = 0;
    update_kbd_wd(report->modifier.val);

    *formatted_data = (uint8_t*)msg;
    *formatted_length = sizeof(*msg);
    return ESP_OK;
}