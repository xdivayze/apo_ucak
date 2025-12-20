#include "network_data_operations.h"

//returns number of failed packets
//size is the number of packets in p_arr
//failed_packet_indices is the callback array with sequence numbers of the packets
int determine_failed_packets(packet **p_arr, size_t size,uint32_t *failed_packet_indices){
    uint32_t n = 0;
    packet* current_packet;
    for(size_t i = 0; i<size; i++) {
        current_packet = p_arr[i];
        if (validate_packet(current_packet)) {
            failed_packet_indices[n] = current_packet->sequence_number;
            n++;
        }
    }
    return n;
}