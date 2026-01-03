idf_component_register(SRCS "main.c"
    PRIV_REQUIRES spi_flash
    INCLUDE_DIRS "" 
    REQUIRES lora_networking_lib utils http_utils testing_utils esp_driver_mcpwm esp_driver_gpio)

