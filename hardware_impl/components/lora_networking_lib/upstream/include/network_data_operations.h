#pragma once

#include "packet.h"
#include <stdbool.h>
//TODO write tests for this header

int data_to_packet_array(packet **callback_buf, uint8_t *data, int data_len,
                        uint32_t dest_addr, uint32_t src_addr, uint16_t ack_id,
                        bool include_handshakes);

//handshakes not included
int packet_array_to_data(packet ** p_buf, uint8_t* data, int len);