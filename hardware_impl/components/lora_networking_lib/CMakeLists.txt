idf_component_register(
    SRCS
        "upstream/src/packet.c"
        "upstream/src/network_data_operations.c"
        "upstream/src/rx_packet_handler.c"
    INCLUDE_DIRS
        "upstream/include"
    REQUIRES 
        log
        )


target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall -Wextra  -g -O0
)

