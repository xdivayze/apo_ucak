#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <stdlib.h>

extern const uint8_t preamble[7];
extern const uint8_t SFD;

extern const uint16_t crc16_polynomial;

extern const size_t payload_length_max;
extern const size_t overhead;
extern const size_t max_frame_size;
typedef struct 
{
    uint32_t dest_address;
    uint32_t src_address;
    uint16_t ack_id;
    uint32_t sequence_number;
    uint8_t payload_length;
    uint8_t *payload;
    uint16_t CRC;
} packet;

packet *packet_constructor(uint32_t dest_address, uint32_t src_address, uint16_t ack_id,
                           uint32_t sequence_number, uint8_t payload_length, uint8_t *payload, uint16_t CRC);

int packet_to_bytestream(uint8_t *buffer, size_t buffer_size, packet *pkt);

int parse_packet(uint8_t *packet_data_raw, packet *p);
int validate_packet(packet *p);
uint16_t calculate_crc(uint8_t *data, size_t length);

#endif