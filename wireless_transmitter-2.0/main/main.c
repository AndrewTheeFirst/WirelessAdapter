#include "wifi.h"
#include "hardware.h"

void app_main(void){
    init_phy();
    connect();
    begin_usb_task();
}

