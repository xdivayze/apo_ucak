cmake_minimum_required(VERSION 3.16)

idf_component_register(
    SRCS
    "src/rx_tests.c" "src/tx_tests.c"
    INCLUDE_DIRS
    "include"
    REQUIRES
    lora_networking_lib
    utils
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall -Wextra -g -O0
)