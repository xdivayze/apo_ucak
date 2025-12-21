idf_component_register(
    SRCS
    "src/spi_utils.c"
    INCLUDE_DIRS
    "include"
    REQUIRES 
    esp_driver_spi

)


target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall -Wextra -g -O0
)