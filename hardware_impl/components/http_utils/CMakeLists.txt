cmake_minimum_required(VERSION 3.16)


idf_component_register(
    SRCS "src/wifi_utils.c" "src/ws_utils.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_wifi esp_event esp_netif esp_websocket_client cjson esp_http_client
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall -Wextra -g -O0
)