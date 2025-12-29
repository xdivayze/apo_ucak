#pragma once

#include "packet.h"
#include <stdbool.h>


int data_to_packet_array(packet **callback_buf, uint8_t *data, int data_len,
                        uint32_t dest_addr, uint32_t src_addr, uint16_t ack_id,
                        bool include_handshakes);