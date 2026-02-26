#include "keyboard.h"
#include "mouse.h"
#include "tusb_device_common.h"
#include "devices.h"

void init_device_queues(){
    init_keyboard_queue();
    init_mouse_queue();
    // init_gamepad_queue();
}

void begin_device_tasks(){
    begin_keyboard_task();
    begin_mouse_task();
    // begin_gamepad_task();
}

void notify_nst_task(uint8_t instance){
    switch(instance){
        case HID_KEYBOARD_INSTANCE:
            notify_mouse_task();
            break;
        case HID_MOUSE_INSTANCE:
            notify_keyboard_task();
            break;
        case HID_GAMEPAD_INSTANCE:
            // notify_gamepad_task();
            break;
    }
}

