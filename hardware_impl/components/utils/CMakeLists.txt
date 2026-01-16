cmake_minimum_required(VERSION 3.16)

idf_component_register(
    SRCS
    "src/spi_utils.c"
    "src/sx_1278_utils.c"
    "src/sx_1278_driver.c"
    INCLUDE_DIRS
    "include"
    REQUIRES
    esp_driver_spi
    lora_networking_lib
    esp_timer
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall -Wextra -g -O0
)