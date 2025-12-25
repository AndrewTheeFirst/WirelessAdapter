#pragma once

#include <stdint.h>

// Message types
typedef enum {
    ESPNOW_MSG_MOUSE_MOVE = 0x01,
    ESPNOW_MSG_MOUSE_CLICK = 0x02,
    ESPNOW_MSG_MOUSE_WHEEL = 0x03,
    ESPNOW_MSG_KEYBOARD_PRESS = 0x10,
    ESPNOW_MSG_KEYBOARD_RELEASE = 0x11,
    ESPNOW_MSG_KEYBOARD_TYPE_STRING = 0x12,
} espnow_msg_type_t;

// Mouse movement message
typedef struct {
    uint8_t msg_type;  // ESPNOW_MSG_MOUSE_MOVE
    int8_t dx;
    int8_t dy;
    int8_t wheel;
    uint8_t buttons;   // bit0=left, bit1=right, bit2=middle
} __attribute__((packed)) espnow_mouse_move_t;

// Mouse click message
typedef struct {
    uint8_t msg_type;  // ESPNOW_MSG_MOUSE_CLICK
    uint8_t buttons;   // Which buttons to press
    uint16_t duration_ms;  // How long to hold
} __attribute__((packed)) espnow_mouse_click_t;

// Keyboard press message (single key)
typedef struct {
    uint8_t msg_type;  // ESPNOW_MSG_KEYBOARD_PRESS
    uint8_t modifiers; // Ctrl, Shift, Alt, etc. 
    uint8_t keycode;   // HID keycode
} __attribute__((packed)) espnow_keyboard_press_t;

typedef struct {
    uint8_t msg_type;  // ESPNOW_MSG_KEYBOARD_RELEASE
    uint8_t keycode;   // Which key to release (0 = release all)
} __attribute__((packed)) espnow_keyboard_release_t;

// Keyboard type string message
typedef struct {
    uint8_t msg_type;  // ESPNOW_MSG_KEYBOARD_TYPE_STRING
    uint8_t length;    // String length (max 248 bytes)
    char text[248];    // Text to type
} __attribute__((packed)) espnow_keyboard_string_t;

// Union for all message types
typedef union {
    uint8_t msg_type;
    espnow_mouse_move_t mouse_move;
    espnow_mouse_click_t mouse_click;
    espnow_keyboard_press_t keyboard_press;
    espnow_keyboard_release_t keyboard_release;
    espnow_keyboard_string_t keyboard_string;
} espnow_message_t;
