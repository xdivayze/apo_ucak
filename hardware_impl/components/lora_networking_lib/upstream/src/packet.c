#include "packet.h"
#include <string.h>

static const size_t addr_length = sizeof(uint32_t);

const size_t overhead = 2 * addr_length + sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint8_t);

const size_t payload_length_max = 8;

const size_t max_frame_size = payload_length_max + overhead;



static void u32_conv_be(uint8_t *dst, uint32_t x)
{
    dst[0] = (uint8_t)(x >> 24);
    dst[1] = (uint8_t)(x >> 16);
    dst[2] = (uint8_t)(x >> 8);
    dst[3] = (uint8_t)(x >> 0);
}

static void u16_conv_be(uint8_t *dst, uint16_t x)
{
    dst[0] = (uint8_t)(x >> 8);
    dst[1] = (uint8_t)(x >> 0);
}

static inline uint32_t read_u32_be(const uint8_t *src)
{
    return ((uint32_t)src[0] << 24) |
           ((uint32_t)src[1] << 16) |
           ((uint32_t)src[2] <<  8) |
           ((uint32_t)src[3] <<  0);
}

static inline uint16_t read_u16_be(const uint8_t *src)
{
    return ((uint16_t)src[0] << 8) |
           ((uint16_t)src[1] << 0);
}

packet_types check_packet_type(packet *p)
{
    if ((p->sequence_number == 0) && (p->payload_length == 0))
        return PACKET_BEGIN;
    if (p->payload_length == 0)
        return PACKET_ACK;
    if ((p->sequence_number == UINT32_MAX) && (p->payload_length == 0))
        return PACKET_END;
    return PACKET_DATA;
}
 bool check_packet_features(packet *p, uint32_t src_addr, uint32_t dest_addr, uint16_t ack_id, uint32_t sequence_number, packet_types packet_type)
{
    if (p->src_address != src_addr)
        return false;
    if (p->dest_address != dest_addr)
        return false;
    if (p->ack_id != ack_id)
        return false;
    if (p->sequence_number != sequence_number)
        return false;
    if (check_packet_type(p) != packet_type)
        return false;
    return true;
}

// does not allocate payload
packet *packet_constructor(uint32_t dest_address, uint32_t src_address, uint16_t ack_id,
                           uint32_t sequence_number, uint8_t payload_length, uint8_t *payload)
{
    if (payload_length > payload_length_max)
        return NULL;

    packet *p = malloc(sizeof(packet));
    p->ack_id = ack_id;
    p->dest_address = dest_address;
    p->payload = payload;
    p->payload_length = payload_length;
    p->sequence_number = sequence_number;
    p->src_address = src_address;
    return p;
}

// returns -1 on fail; packet size on success
int parse_packet(uint8_t *packet_data_raw, packet *p)
{
    size_t idx = 0;

    uint32_t dest_addr = read_u32_be(&packet_data_raw[idx]);
    idx += addr_length;

    uint32_t src_addr= read_u32_be(&packet_data_raw[idx]);
    idx += addr_length;

    uint16_t ack_id= read_u16_be(&packet_data_raw[idx]);
    idx += sizeof(uint16_t);

    uint32_t sequence_number= read_u32_be(&packet_data_raw[idx]);
    idx += sizeof(sequence_number);

    uint8_t payload_length = packet_data_raw[idx];
    idx++;

    uint8_t *payload = malloc(payload_length);
    memcpy(payload, &(packet_data_raw[idx]), payload_length);
    idx += payload_length;

    p->ack_id = ack_id;
    p->dest_address = dest_addr;
    p->payload = payload;
    p->payload_length = payload_length;
    p->sequence_number = sequence_number;
    p->src_address = src_addr;

    return idx;
}

// free packet and its payload
void free_packet(packet *p)
{
    if (p->payload)
        free(p->payload);
    free(p);
}

packet *ack_packet(uint32_t dest_address, uint32_t src_address, uint16_t ack_id,
                   uint32_t sequence_number) {
                    return packet_constructor(dest_address, src_address, ack_id, sequence_number, 0, NULL);
                   }

// returns -1 on fail; packet size on success
// writes pkt to buffer if buffer_size > pkt's frame size
int packet_to_bytestream(uint8_t *buffer, size_t buffer_size, packet *pkt)
{

    if (buffer_size < (overhead + pkt->payload_length))
        return -1;

    size_t idx = 0;
    u32_conv_be(&(buffer[idx]), pkt->dest_address);
    idx += addr_length;
    u32_conv_be(&(buffer[idx]), pkt->src_address);
    idx += addr_length;
    u16_conv_be(&(buffer[idx]), pkt->ack_id);
    idx += sizeof(pkt->ack_id);
    u32_conv_be(&(buffer[idx]), pkt->sequence_number);
    idx += sizeof(pkt->sequence_number);
    buffer[idx] = pkt->payload_length;
    idx += sizeof(pkt->payload_length);

    memcpy(&(buffer[idx]), pkt->payload, pkt->payload_length);
    idx += pkt->payload_length;

    return idx;
}