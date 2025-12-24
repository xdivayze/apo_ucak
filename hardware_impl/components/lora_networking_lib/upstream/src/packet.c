#include "packet.h"
#include <string.h>

const uint8_t preamble[7] = {'s', 'e', 'j', 'u', 'a', 'n', 'i'};
const uint8_t SFD = 0b00110011;

static const size_t addr_length = sizeof(uint32_t);

const size_t overhead = sizeof(preamble) + sizeof(uint16_t) + sizeof(SFD) + 2 * addr_length + sizeof(uint32_t) + sizeof(uint8_t);

const size_t payload_length_max = 8;

const size_t max_frame_size = payload_length_max + overhead;

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

    for (size_t i = 0; i < sizeof(preamble); i++)
    {
        if (packet_data_raw[idx] != preamble[i])
            return -1;
        idx++;
    }

    if (packet_data_raw[idx] != SFD)
        return -1;
    idx++;

    uint32_t dest_addr;
    memcpy(&dest_addr, &(packet_data_raw[idx]), addr_length);
    idx += addr_length;

    uint32_t src_addr;
    memcpy(&src_addr, &(packet_data_raw[idx]), addr_length);
    idx += addr_length;

    uint16_t ack_id;
    memcpy(&ack_id, &(packet_data_raw[idx]), sizeof(uint16_t));
    idx += sizeof(uint16_t);

    uint32_t sequence_number;
    memcpy(&sequence_number, &(packet_data_raw[idx]), sizeof(sequence_number));
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

// returns -1 on fail; packet size on success
// writes pkt to buffer if buffer_size > pkt's frame size
int packet_to_bytestream(uint8_t *buffer, size_t buffer_size, packet *pkt)
{

    if (buffer_size < (overhead + pkt->payload_length))
        return -1;

    size_t idx = 0;
    memcpy(&(buffer[0]), preamble, sizeof(preamble));
    idx += sizeof(preamble);
    buffer[idx] = SFD;
    idx += sizeof(SFD);
    memcpy(&(buffer[idx]), &(pkt->dest_address), addr_length);
    idx += addr_length;
    memcpy(&(buffer[idx]), &(pkt->src_address), addr_length);
    idx += addr_length;
    memcpy(&buffer[idx], &(pkt->ack_id), sizeof(pkt->ack_id));
    idx += sizeof(pkt->ack_id);
    buffer[idx] = pkt->sequence_number;
    idx += sizeof(pkt->sequence_number);
    buffer[idx] = pkt->payload_length;
    idx += sizeof(pkt->payload_length);

    memcpy(&(buffer[idx]), pkt->payload, pkt->payload_length);
    idx += pkt->payload_length;

    return idx;
}