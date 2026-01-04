#pragma once
#include <stdint.h>

// Message Types
typedef enum {
    ESPNOW_MSG_MOUSE = 0x01,
    ESPNOW_MSG_KEYBOARD = 0x02,
    ESPNOW_MSG_GAMEPAD = 0x04,
} espnow_msg_type_t;

// Message types choose to follow HID spec as defined by HID.h
typedef struct {
    uint8_t msg_type;   // ESPNOW_MSG_MOUSE
    uint8_t buttons;    // Clicks: Left, Right, Middle, etc.
    int8_t x;           // Horizontal Movement Dx
    int8_t y;           // Vertical Movement Dy
    int8_t wheel;
    int8_t pan;
} __attribute__((packed)) espnow_msg_mouse_t;

typedef struct {
    uint8_t msg_type;       // ESPNOW_MSG_KEYBOARD
    uint8_t modifiers;      // Ctrl, Shift, Alt, etc.
    uint8_t reserved;       // Unused
    uint8_t keys[6];        // HID keycodes
} __attribute__((packed)) espnow_msg_keyboard_t;

typedef struct {
    uint8_t msg_type;       // ESPNOW_MSG_GAMEPAD
    int8_t  x;              // Delta x  movement of left analog-stick
    int8_t  y;              // Delta y  movement of left analog-stick
    int8_t  z;              // Delta z  movement of right analog-joystick
    int8_t  rz;             // Delta Rz movement of right analog-joystick
    int8_t  rx;             // Delta Rx movement of analog left trigger
    int8_t  ry;             // Delta Ry movement of analog right trigger
    uint8_t hat;            // Buttons mask for currently pressed buttons in the DPad/hat
    uint32_t buttons;       // Buttons mask for currently pressed buttons
} __attribute__((packed)) espnow_msg_gamepad_t;

// Union for all message types
typedef union {
    uint8_t msg_type; // Acts as a header
    espnow_msg_mouse_t mouse_msg;
    espnow_msg_keyboard_t keyboard_msg;
    espnow_msg_gamepad_t gamepad_msg;
} espnow_message_t;
