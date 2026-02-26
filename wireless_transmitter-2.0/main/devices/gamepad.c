#include <stdint.h>
#include <stddef.h>
#include "msg_types.h"
#include "constants.h"

#define TU_BIT(n) (1UL << (n))

typedef enum {
    CONTROLLER_TYPE_UNKNOWN,
    CONTROLLER_TYPE_SAITEK,
    CONTROLLER_TYPE_SAITEK_P2500
} controller_type_t;

typedef enum {
    GAMEPAD_HAT_CENTERED   = 0,  ///< DPAD_CENTERED
    GAMEPAD_HAT_UP         = 1,  ///< DPAD_UP
    GAMEPAD_HAT_UP_RIGHT   = 2,  ///< DPAD_UP_RIGHT
    GAMEPAD_HAT_RIGHT      = 3,  ///< DPAD_RIGHT
    GAMEPAD_HAT_DOWN_RIGHT = 4,  ///< DPAD_DOWN_RIGHT
    GAMEPAD_HAT_DOWN       = 5,  ///< DPAD_DOWN
    GAMEPAD_HAT_DOWN_LEFT  = 6,  ///< DPAD_DOWN_LEFT
    GAMEPAD_HAT_LEFT       = 7,  ///< DPAD_LEFT
    GAMEPAD_HAT_UP_LEFT    = 8,  ///< DPAD_UP_LEFT
} hid_gamepad_hat_t;

typedef enum {
    GAMEPAD_BUTTON_0  = TU_BIT(0),
    GAMEPAD_BUTTON_1  = TU_BIT(1),
    GAMEPAD_BUTTON_2  = TU_BIT(2),
    GAMEPAD_BUTTON_3  = TU_BIT(3),
    GAMEPAD_BUTTON_4  = TU_BIT(4),
    GAMEPAD_BUTTON_5  = TU_BIT(5),
    GAMEPAD_BUTTON_6  = TU_BIT(6),
    GAMEPAD_BUTTON_7  = TU_BIT(7),
    GAMEPAD_BUTTON_8  = TU_BIT(8),
    GAMEPAD_BUTTON_9  = TU_BIT(9),
    GAMEPAD_BUTTON_10 = TU_BIT(10),
    GAMEPAD_BUTTON_11 = TU_BIT(11),
    GAMEPAD_BUTTON_12 = TU_BIT(12),
    GAMEPAD_BUTTON_13 = TU_BIT(13),
    GAMEPAD_BUTTON_14 = TU_BIT(14),
    GAMEPAD_BUTTON_15 = TU_BIT(15),
    GAMEPAD_BUTTON_16 = TU_BIT(16),
    GAMEPAD_BUTTON_17 = TU_BIT(17),
    GAMEPAD_BUTTON_18 = TU_BIT(18),
    GAMEPAD_BUTTON_19 = TU_BIT(19),
    GAMEPAD_BUTTON_20 = TU_BIT(20),
    GAMEPAD_BUTTON_21 = TU_BIT(21),
    GAMEPAD_BUTTON_22 = TU_BIT(22),
    GAMEPAD_BUTTON_23 = TU_BIT(23),
    GAMEPAD_BUTTON_24 = TU_BIT(24),
    GAMEPAD_BUTTON_25 = TU_BIT(25),
    GAMEPAD_BUTTON_26 = TU_BIT(26),
    GAMEPAD_BUTTON_27 = TU_BIT(27),
    GAMEPAD_BUTTON_28 = TU_BIT(28),
    GAMEPAD_BUTTON_29 = TU_BIT(29),
    GAMEPAD_BUTTON_30 = TU_BIT(30),
    GAMEPAD_BUTTON_31 = TU_BIT(31),
} hid_gamepad_button_bm_t;

typedef enum {
    GAMEPAD_BUTTON_SOUTH   = GAMEPAD_BUTTON_0,
    GAMEPAD_BUTTON_EAST    = GAMEPAD_BUTTON_1,
    GAMEPAD_BUTTON_C       = GAMEPAD_BUTTON_2,
    GAMEPAD_BUTTON_NORTH   = GAMEPAD_BUTTON_3,
    GAMEPAD_BUTTON_WEST    = GAMEPAD_BUTTON_4,
    GAMEPAD_BUTTON_Z       = GAMEPAD_BUTTON_5,
    GAMEPAD_BUTTON_TL      = GAMEPAD_BUTTON_6,
    GAMEPAD_BUTTON_TR      = GAMEPAD_BUTTON_7,
    GAMEPAD_BUTTON_TL2     = GAMEPAD_BUTTON_8,
    GAMEPAD_BUTTON_TR2     = GAMEPAD_BUTTON_9,
    GAMEPAD_BUTTON_SELECT  = GAMEPAD_BUTTON_10,
    GAMEPAD_BUTTON_START   = GAMEPAD_BUTTON_11,
    GAMEPAD_BUTTON_MODE    = GAMEPAD_BUTTON_12,
    GAMEPAD_BUTTON_THUMBL  = GAMEPAD_BUTTON_13,
    GAMEPAD_BUTTON_THUMBR  = GAMEPAD_BUTTON_14
} std_gamepad_button_bm_t;

// SAITEK P2500
typedef struct {
    int8_t report_id;
    uint8_t lt_joystk_hor;  // Absolute w/ 0x80 (128) center  
    uint8_t lt_joystk_vert; // Absolute w/ 0x80 (128) center     
    uint8_t rt_joystk_hor;  // Absolute w/ 0x80 (128) center  
    uint8_t rt_joystk_vert; // Absolute w/ 0x80 (128) center     
    uint8_t buttons; // Main 8 buttons
    uint8_t special; // special is broken into 2Nib -> hat_switch = 0x[0-7]0, meta_buttons = 0x0[0-3]
} __attribute__((packed)) saitek_controller_report;

typedef enum{
    // Contained within Button Field
    SAITEK_P2500_BUTTON_WEST    = TU_BIT(0),
    SAITEK_P2500_BUTTON_NORTH   = TU_BIT(1),
    SAITEK_P2500_BUTTON_SOUTH   = TU_BIT(2),
    SAITEK_P2500_BUTTON_EAST    = TU_BIT(3),
    SAITEK_P2500_BUTTON_5       = TU_BIT(4),
    SAITEK_P2500_BUTTON_6       = TU_BIT(5),
    SAITEK_P2500_BUTTON_TL      = TU_BIT(6),
    SAITEK_P2500_BUTTON_TR      = TU_BIT(7),
    // Contained within Special Field
    SAITEK_P2500_BUTTON_THUMBL  = TU_BIT(0),
    SAITEK_P2500_BUTTON_THUMBR  = TU_BIT(1),
    SAITEK_P2500_BUTTON_START   = TU_BIT(2),
    SAITEK_P2500_BUTTON_SELECT  = TU_BIT(3)
} saitek_button_bm_t;

// use (report->special & 0xF0) to extract
typedef enum{
    SAITEK_PS2500_HAT_CENTERED   = 0xF0,
    SAITEK_PS2500_HAT_UP         = 0x00,
    SAITEK_PS2500_HAT_UP_RIGHT   = 0x10,
    SAITEK_PS2500_HAT_RIGHT      = 0x20,
    SAITEK_PS2500_HAT_DOWN_RIGHT = 0x30,
    SAITEK_PS2500_HAT_DOWN       = 0x40,
    SAITEK_PS2500_HAT_DOWN_LEFT  = 0x50,
    SAITEK_PS2500_HAT_LEFT       = 0x60,
    SAITEK_PS2500_HAT_UP_LEFT    = 0x70
} saitek_hat_t;

hid_gamepad_hat_t convert_saitek_hat(uint8_t hat);
uint32_t convert_saitek_buttons(uint8_t buttons, uint8_t special);

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

#if !NOT_IMPLEMENTED
static uint8_t* process_gamepad_report(const uint8_t* data, size_t length) {
    // TODO . . .
    return (uint8_t*)0;
}
#endif

#if NOT_IMPLEMENTED
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
    esp_now_send(transmitter_peer_mac, (uint8_t*)&msg, sizeof(msg));
}
#endif
