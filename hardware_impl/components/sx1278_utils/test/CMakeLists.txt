cmake_minimum_required(VERSION 3.16)

idf_component_register(
  SRCS 
  "test_sx_1278_config.c"
  INCLUDE_DIRS "."
  REQUIRES sx1278_utils unity esp_driver_gpio
)
