#include <stdint.h>
#include "controller_usage.h"

#define MAKE_KEY(vid, pid) (((uint32_t)(vid) << 16) | (pid))
#define HAT_UP      (1 << 0)
#define HAT_RIGHT   (1 << 1)
#define HAT_DOWN    (1 << 2)
#define HAT_LEFT    (1 << 3)


// Primary lookup:   Specific VID+PID combination
static controller_type_t lookup_specific(uint16_t vid, uint16_t pid) {
    uint32_t key = MAKE_KEY(vid, pid);
    
    switch (key) {
        case MAKE_KEY(0x06A3, 0xFF0C)   : return CONTROLLER_TYPE_SAITEK_P2500;
        default                         : return CONTROLLER_TYPE_UNKNOWN;  // Not found
    }
}

static controller_type_t lookup_by_pid(uint16_t pid) {
    switch (pid) {
        case 0x06A3 : return CONTROLLER_TYPE_SAITEK;
        default     : return CONTROLLER_TYPE_UNKNOWN;
    }
}

void identify_controller(uint16_t vid, uint16_t pid, controller_type_t* controller_type) {
    *controller_type = lookup_specific(vid, pid); // Try VID + PID lookup
    if (*controller_type == CONTROLLER_TYPE_UNKNOWN)
        *controller_type = lookup_by_pid(pid); // Use fallback PID lookup
}

hid_gamepad_hat_t convert_saitek_hat(uint8_t hat){
    switch(hat){
        case SAITEK_PS2500_HAT_UP:          return GAMEPAD_HAT_UP;
        case SAITEK_PS2500_HAT_UP_RIGHT:    return GAMEPAD_HAT_UP_RIGHT;
        case SAITEK_PS2500_HAT_RIGHT:       return GAMEPAD_HAT_RIGHT;
        case SAITEK_PS2500_HAT_DOWN_RIGHT:  return GAMEPAD_HAT_DOWN_RIGHT;
        case SAITEK_PS2500_HAT_DOWN:        return GAMEPAD_HAT_DOWN;
        case SAITEK_PS2500_HAT_DOWN_LEFT:   return GAMEPAD_HAT_DOWN_LEFT;
        case SAITEK_PS2500_HAT_LEFT:        return GAMEPAD_HAT_LEFT;
        case SAITEK_PS2500_HAT_UP_LEFT:     return GAMEPAD_HAT_UP_LEFT;
        case SAITEK_PS2500_HAT_CENTERED:
        default:                            return GAMEPAD_HAT_CENTERED;
    }
}

uint32_t convert_saitek_buttons(uint8_t report_buttons, uint8_t special){
    uint32_t buttons = 0;
    buttons |= (report_buttons & SAITEK_P2500_BUTTON_WEST)  ? GAMEPAD_BUTTON_WEST   : 0;
    buttons |= (report_buttons & SAITEK_P2500_BUTTON_NORTH) ? GAMEPAD_BUTTON_NORTH  : 0;
    buttons |= (report_buttons & SAITEK_P2500_BUTTON_SOUTH) ? GAMEPAD_BUTTON_SOUTH  : 0;
    buttons |= (report_buttons & SAITEK_P2500_BUTTON_EAST)  ? GAMEPAD_BUTTON_EAST   : 0;
    buttons |= (report_buttons & SAITEK_P2500_BUTTON_5)     ? GAMEPAD_BUTTON_C      : 0;
    buttons |= (report_buttons & SAITEK_P2500_BUTTON_6)     ? GAMEPAD_BUTTON_Z      : 0;
    buttons |= (report_buttons & SAITEK_P2500_BUTTON_TL)    ? GAMEPAD_BUTTON_TL     : 0;
    buttons |= (report_buttons & SAITEK_P2500_BUTTON_TR)    ? GAMEPAD_BUTTON_TR     : 0;
    buttons |= (special & SAITEK_P2500_BUTTON_THUMBL)       ? GAMEPAD_BUTTON_THUMBL : 0;
    buttons |= (special & SAITEK_P2500_BUTTON_THUMBR)       ? GAMEPAD_BUTTON_THUMBR : 0;
    buttons |= (special & SAITEK_P2500_BUTTON_START)        ? GAMEPAD_BUTTON_START  : 0;
    buttons |= (special & SAITEK_P2500_BUTTON_SELECT)       ? GAMEPAD_BUTTON_SELECT : 0;
    return buttons;
}
// Helper to get type name
const char* get_type_name(controller_type_t type) {
    switch (type) {
        case CONTROLLER_TYPE_SAITEK         :
        case CONTROLLER_TYPE_SAITEK_P2500   : return "Saitek";
        default                             : return "Unknown";
    }
}