#include "packet.h"
#include "string.h"

const uint8_t preamble[7] = {'s', 'e', 'j', 'u', 'a', 'n', 'i'};
const uint8_t SFD = 0b00110011;

const uint16_t crc16_polynomial = 0x1021;
static const size_t crc_length = 2;

static const size_t addr_length = sizeof(uint32_t);

const size_t overhead = sizeof(preamble) + sizeof(SFD) + 2 * addr_length + sizeof(uint32_t) + sizeof(uint8_t) + crc_length;

const size_t payload_length_max = 8;

const size_t max_frame_size = payload_length_max + overhead;

packet *packet_constructor(uint32_t dest_address, uint32_t src_address,
                           uint32_t sequence_number, uint8_t payload_length, uint8_t *payload, uint16_t CRC)
{
    if (payload_length > payload_length_max)
        return NULL;

    packet *p = malloc(sizeof(packet));
    p->CRC = CRC;
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

    uint32_t sequence_number;
    memcpy(&sequence_number, &(packet_data_raw[idx]), sizeof(sequence_number));
    idx += sizeof(sequence_number);

    uint8_t payload_length = packet_data_raw[idx];
    idx++;

    uint8_t *payload = malloc(payload_length);
    memcpy(payload, &(packet_data_raw[idx]), payload_length);
    idx += payload_length;

    uint16_t crc;
    memcpy(&crc, &(packet_data_raw[idx]), crc_length);
    idx += crc_length;

    p->CRC = crc;
    p->dest_address = dest_addr;
    p->payload = payload;
    p->payload_length = payload_length;
    p->sequence_number = sequence_number;
    p->src_address = src_addr;

    return idx;
}

int validate_packet(packet *p)
{
    uint16_t calculated_crc = calculate_crc(p->payload, p->payload_length);
    if (calculated_crc != p->CRC)
        return -1;
    return 0;
}

// returns -1 on fail; packet size on success
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
    buffer[idx] = pkt->sequence_number;
    idx += sizeof(pkt->sequence_number);
    buffer[idx] = pkt->payload_length;
    idx += sizeof(pkt->payload_length);

    memcpy(&(buffer[idx]), pkt->payload, pkt->payload_length);
    idx += pkt->payload_length;
    memcpy(&(buffer[idx]), &(pkt->CRC), crc_length);
    idx += crc_length;

    return idx;
}

uint16_t calculate_crc(uint8_t *data, size_t length)
{
    uint8_t i;
    uint16_t wCrc = 0xffff;
    while (length--)
    {
        wCrc ^= *(unsigned char *)data++ << 8;
        for (i = 0; i < 8; i++)
            wCrc = wCrc & 0x8000 ? (wCrc << 1) ^ crc16_polynomial : wCrc << 1;
    }
    return wCrc & 0xffff;
}