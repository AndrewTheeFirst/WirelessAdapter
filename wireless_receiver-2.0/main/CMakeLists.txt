idf_component_register(
    SRCS 
        "main.c"
        "devices/keyboard.c"
        "devices/mouse.c"
        "devices/devices.c"
        "tusb/tusb_cb.c"
        "hardware/hardware.c"

    PRIV_INCLUDE_DIRS
        "."
        "tusb"
        "devices"
        "hardware"
    PRIV_REQUIRES
        wireless_shared
        nvs_flash
        esp_wifi
        esp_timer
        tinyusb
        usb
)

idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
target_include_directories(${tusb_lib} PRIVATE ./tusb)
target_link_libraries(${tusb_lib} INTERFACE idf::main)