#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <stdlib.h>

extern const size_t payload_length_max;
extern const size_t overhead;
extern const size_t max_frame_size;
typedef enum
{
    PACKET_ACK = 0, //payload length is 0 and sequence number is non zero
    PACKET_BEGIN = 1, //payload length and sequence number is 0
    PACKET_END = 2, //sequence number is uint32_max and payload length is 0
    PACKET_DATA = 3, 
} packet_types;

typedef struct
{
    uint32_t dest_address;
    uint32_t src_address;
    uint16_t ack_id;
    uint32_t sequence_number;
    uint8_t payload_length;
    uint8_t *payload;
} packet;

//TODO
packet *ack_packet(uint32_t dest_address, uint32_t src_address, uint16_t ack_id,
                   uint32_t sequence_number);

packet *packet_constructor(uint32_t dest_address, uint32_t src_address, uint16_t ack_id,
                           uint32_t sequence_number, uint8_t payload_length, uint8_t *payload);
void free_packet(packet *p);

int packet_to_bytestream(uint8_t *buffer, size_t buffer_size, packet *pkt);

int parse_packet(uint8_t *packet_data_raw, packet *p);



#endif