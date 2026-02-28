#include "esp_timer.h"
#include "esp_log.h"
#include "device_config.h"

#define US_IN_S 1000000ULL
#define LIGHT_SLEEP_DUR_US (LIGHT_SLEEP_DUR_S * US_IN_S)
#define DEEP_SLEEP_DUR_US (DEEP_SLEEP_DUR_S * US_IN_S)

static const char* TAG = "USB_TRANSMITTER // sleep.c";

#if SLEEP
static esp_timer_handle_t lsm_timer;
static void enter_lsm(void){
    ESP_LOGI(TAG, "Entering Light Sleep");
    ESP_LOGI(TAG, "Exiting Light Sleep");
    esp_timer_start_once(lsm_timer, LIGHT_SLEEP_DUR_US);
}
#if DEEP_SLEEP
static esp_timer_handle_t dsm_timer;
static void enter_dsm(void){
    ESP_LOGI(TAG, "Entering Deep Sleep");
    ESP_LOGI(TAG, "Exiting Deep Sleep");
    esp_timer_start_once(dsm_timer, DEEP_SLEEP_DUR_US);
}
#endif
#endif

void start_sleep_timers(void){
    #if SLEEP
    esp_timer_create_args_t lsm_timer_args = { .callback = enter_lsm };
    esp_timer_create(&lsm_timer_args, &lsm_timer);
    esp_timer_start_once(lsm_timer, LIGHT_SLEEP_DUR_US);
    #if DEEP_SLEEP
    esp_timer_create_args_t dsm_timer_args = { .callback = enter_dsm };
    esp_timer_create(&dsm_timer_args, &dsm_timer);
    esp_timer_start_once(dsm_timer, DEEP_SLEEP_DUR_US);
    #endif
    #endif
}

void reset_timers(void){
    #if SLEEP
    esp_timer_restart(lsm_timer, LIGHT_SLEEP_DUR_US);
    #if DEEP_SLEEP
    esp_timer_restart(dsm_timer, DEEP_SLEEP_DUR_US);
    #endif
    #endif
}