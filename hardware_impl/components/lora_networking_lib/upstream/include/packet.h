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
typedef enum
{
    PACKET_ACK = 0, //payload length is 0 and sequence number is non zero
    PACKET_BEGIN = 1, //payload length and sequence number is 0
    PACKET_END = 2, //payload length is uint32_max
    PACKET_DATA = 3, 
} packet_types;

typedef struct // TODO remove CRC
{
    uint32_t dest_address;
    uint32_t src_address;
    uint16_t ack_id;
    uint32_t sequence_number;
    uint8_t payload_length;
    uint8_t *payload;
    uint16_t CRC;
} packet;

packet *ack_packet(uint32_t dest_address, uint32_t src_address, uint16_t ack_id,
                   uint32_t sequence_number);

packet *packet_constructor(uint32_t dest_address, uint32_t src_address, uint16_t ack_id,
                           uint32_t sequence_number, uint8_t payload_length, uint8_t *payload, uint16_t CRC);
void free_packet(packet *p);

int packet_to_bytestream(uint8_t *buffer, size_t buffer_size, packet *pkt);

int parse_packet(uint8_t *packet_data_raw, packet *p);
int validate_packet(packet *p);
uint16_t calculate_crc(uint8_t *data, size_t length);

#endif