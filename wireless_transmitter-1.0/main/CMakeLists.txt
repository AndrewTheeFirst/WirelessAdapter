idf_component_register(
    SRCS 
        "main.c"
    INCLUDE_DIRS 
        "."
        "tusb"
        "../../common"
    REQUIRES
        espressif__tinyusb 
        usb
        esp_hw_support
        nvs_flash
        esp_wifi
        driver
)

idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
target_include_directories(${tusb_lib} PUBLIC "${CMAKE_CURRENT_LIST_DIR}/tusb")
target_link_libraries(${tusb_lib} INTERFACE idf::main)