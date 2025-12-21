#ifndef NETWORK_DATA_OPERATIONS_H
#define NETWORK_DATA_OPERATIONS_H

#include "packet.h"
int determine_failed_packets(packet **p_arr, size_t size,uint32_t *failed_packet_indices);

#endif