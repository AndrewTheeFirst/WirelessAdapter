#include "esp_private/usb_phy.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "tusb.h"

static const char* TAG = "USB_RECEIVER // hardware.c";

usb_phy_handle_t phy_hdl;

void init_phy(void){
    
    usb_phy_config_t phy_config = {
        .controller     = USB_PHY_CTRL_OTG,
        .target         = USB_PHY_TARGET_INT,
        .otg_mode       = USB_OTG_MODE_DEVICE,
        .otg_speed      = USB_PHY_SPEED_FULL,
        .ext_io_conf    = NULL,
        .otg_io_conf    = NULL};

    ESP_ERROR_CHECK(usb_new_phy(&phy_config, &phy_hdl));
}

void wait_for_mount(void){
    ESP_LOGI(TAG, "Waiting for USB to mount...");
    while (!tud_mounted()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(TAG, "USB mounted! Device is ready.");
}

// Task to process and deliver USB events to host
// tud_task() wrapper to control polling rate
static void usb_tud_task(void* arg){
    ESP_LOGI(TAG, "TinyUSB device task started");
    while (true) {
        tud_task();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

esp_err_t begin_usb_tud(void){
    if (tusb_init()) {
        if(xTaskCreate(usb_tud_task, "tud_task", 4096, NULL, 4, NULL) == pdPASS)
            return ESP_OK;
    }
    return ESP_FAIL;
}