idf_component_register(
    SRCS
        "upstream/src/packet.c"
        "upstream/src/network_data_operations.c"
        "upsteam/src/rx_packet_handler.c"
    INCLUDE_DIRS
        "upstream/include"
)


target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall -Wextra  -g -O0
)

