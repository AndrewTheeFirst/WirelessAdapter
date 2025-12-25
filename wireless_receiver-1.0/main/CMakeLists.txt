idf_component_register(
    SRCS
        "main.c" 
        "tusb/usb_descriptors.c"
    INCLUDE_DIRS
        "." 
        "tusb"
        "../../common"
    REQUIRES
        espressif__tinyusb 
        usb
        driver
        esp_hw_support
        esp_wifi
        nvs_flash
)

idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
target_include_directories(${tusb_lib} PUBLIC "${CMAKE_CURRENT_LIST_DIR}/tusb")
target_link_libraries(${tusb_lib} INTERFACE idf::main)