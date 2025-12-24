#include "sx_1278_utils.h"
#include "spi_utils.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "esp_err.h"
#include "esp_timer.h"
#include <stdbool.h>

#define FXOSC 32000000UL

#define lora_frequency_lf 410000000UL // datasheet table 32 for frequency bands
#define lora_bandwidth_hz 125000UL    // table 12 for spreading factor-bandwidth relations
#define spreading_factor 12
static packet_types check_packet_type(packet *p)
{
    if ((p->sequence_number == 0) && (p->payload_length == 0))
        return PACKET_BEGIN;
    if (p->payload_length == 0)
        return PACKET_ACK;
    if ((p->sequence_number == UINT32_MAX) && (p->payload_length == 0))
        return PACKET_END;
    return PACKET_DATA;
}
static bool check_packet_features(packet *p, uint32_t src_addr, uint32_t dest_addr, uint16_t ack_id, uint32_t sequence_number, packet_types packet_type)
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

// read from BEGIN to END packets sending ACKs in between
// discards anything not intended for host address
// TODO add adjustable size vector to temporarily store the packets then return
esp_err_t read_burst(packet **p_buf, int *len, int handshake_timeout, uint32_t host_addr)
{
    int n = -1;
    uint8_t data = 0;
    packet *p = malloc(sizeof(packet));
    if (!p)
        return ESP_ERR_NO_MEM;
    data = 0b10001110;
    esp_err_t ret = spi_burst_write_reg(sx_1278_spi, 0x01, &data, 1); // put into rx
    uint32_t target_addr = 0;
    uint16_t ack_id = 0;
    int repeat = (int)round(handshake_timeout / 2000.0);
    for (int i = 0; i < repeat; i++)
    {

        ret = poll_for_irq_flag(2000, 2, 1 << 6, false);
        if (ret != ESP_OK)
            continue;
        data = 0b10001001;
        ret = spi_burst_write_reg(sx_1278_spi, 0x01, &data, 1); // put in standby mode
        ret = read_last_packet(p);
        if (ret != ESP_OK)
            continue;
        if (check_packet_features(p, p->src_address, p->dest_address, p->ack_id, 0, PACKET_BEGIN)) // accept if destination address matches and is a valid handshake packet
        {
            target_addr = p->src_address; // set the source address
            ack_id = p->ack_id;           // set the ack id
            n = 0;
            ret = send_packet(ack_packet(target_addr, host_addr, ack_id, p->sequence_number), true);
            if (ret != ESP_OK)
                continue;
            break;
        }
        data = 0b10001110;
        ret = spi_burst_write_reg(sx_1278_spi, 0x01, &data, 1); // put into rx
    }

    if (n != 0)
    {
        data = 0b10001001;
        ret = spi_burst_write_reg(sx_1278_spi, 0x01, &data, 1); // put in standby mode
        free_packet(p);
        return ESP_ERR_TIMEOUT;
    }
    bool end_reached = false;
    while (!end_reached)
    {
        for (int i = 0; i < (repeat + 1); i++)
        {
            if (i == repeat)
            {
                free_packet(p);
                return ESP_ERR_TIMEOUT;
            }
            data = 0b10001110;
            ret = spi_burst_write_reg(sx_1278_spi, 0x01, &data, 1); // put into rx
            ret = poll_for_irq_flag(2000, 2, 1 << 6, false);
            if (ret != ESP_OK)
                continue;
            data = 0b10001001;
            ret = spi_burst_write_reg(sx_1278_spi, 0x01, &data, 1); // put in standby mode
            ret = read_last_packet(p);
            if (ret != ESP_OK)
                continue;

            if (check_packet_features(p, target_addr, host_addr, ack_id, n, PACKET_DATA)) // check if nth data packet
            {
                ret = send_packet(ack_packet(target_addr, host_addr, ack_id, n), true);
                if (ret != ESP_OK)
                    continue;                        // if ACK is not sent repeat everything
                memcpy(p_buf[n], p, sizeof(packet)); // only update buffer if ack is sent
                n++;
                break;
            }
            else if (check_packet_features(p, target_addr, host_addr, ack_id, UINT32_MAX, PACKET_END))
            {
                ret = send_packet(ack_packet(target_addr, host_addr, ack_id, UINT32_MAX), false); // end packet continue at standby
                if (ret != ESP_OK)
                    continue; // if ACK is not sent repeat everything
                end_reached = true;
                break;
            }
        }
    }
    free_packet(p);

    *len = n;
    return ESP_OK;
}

// this one only sends packets in the array and waits for acks in between always include the BEGIN and END packets in the array
esp_err_t send_burst(packet **p_buf, const int len)
{
    packet *curr_packet;
    esp_err_t ret;
    uint8_t data = 0;
    packet *rx_packet = malloc(sizeof(packet));
    for (int i = 0; i < len; i++)
    {
        while (1)
        {
            memset(rx_packet, 0, sizeof(*rx_packet));
            curr_packet = p_buf[i];
            ret = send_packet(curr_packet, 1);

            if (ret == ESP_ERROR_CHECK_WITHOUT_ABORT(0))
            {
                data = 0b10001110;
                ret = spi_burst_write_reg(sx_1278_spi, 0x01, &data, 1);
                if (ret != ESP_OK)
                {
                    fprintf(stderr, "retry failed when setting sx1278 to rx mode\n");
                    free_packet(rx_packet);
                    return ret;
                }
            }
            else if (ret != ESP_OK)
            {
                free_packet(rx_packet);
                return ret;
            }

            ret = poll_for_irq_flag(2000, 2, 1 << 6, false); // do not clear irq flags as read_last_packet needs them
            if (ret != ESP_OK)                               // if polling resulted in rxdone flag not being set retry
                continue;

            ret = read_last_packet(rx_packet);
            if (ret != ESP_OK)
            { // retry if ack packet integrity cannot be verified
                fprintf(stderr, "ack packet failed to be read, retrying\n");
                continue;
            }
            if (check_packet_features(rx_packet, curr_packet->dest_address, curr_packet->src_address, curr_packet->ack_id, curr_packet->sequence_number, PACKET_ACK))
            { // ith ACK packet
                break;
            }
            else
                continue; // retry if not ack packet
        }
    }
    free_packet(rx_packet);
    return ESP_OK;
}

esp_err_t poll_for_irq_flag(size_t timeout_ms, size_t poll_interval_ms, uint8_t irq_and_mask, bool cleanup)
{
    timeout_ms = (timeout_ms <= 0) ? 3000 : timeout_ms;
    poll_interval_ms = (poll_interval_ms <= 0) ? 2 : poll_interval_ms;

    const int64_t start = esp_timer_get_time();
    const int64_t timeout_us = (int64_t)timeout_ms * 1000;

    uint8_t irq = 0;
    esp_err_t ret;

    int64_t elapsed_us = 0;
    uint8_t data;
    while (1)
    {
        ret = spi_burst_read_reg(sx_1278_spi, 0x12, &irq, 1);
        if (ret != ESP_OK)
        {
            fprintf(stderr, "couldnt read irq register\n");
            if (cleanup)
            {
                data = 0xFF;
                spi_burst_write_reg(sx_1278_spi, 0x12, &data, 1); // clear irq flags
            }
            return ret;
        }
        if (irq & irq_and_mask)
        {
            if (cleanup)
            {
                data = 0xFF;
                spi_burst_write_reg(sx_1278_spi, 0x12, &data, 1); // clear irq flags
            }
            return ESP_OK;
        }

        elapsed_us = esp_timer_get_time() - start;
        if (elapsed_us > timeout_us)
        {
            if (cleanup)
            {
                data = 0xFF;
                spi_burst_write_reg(sx_1278_spi, 0x12, &data, 1); // clear irq flags
            }
            fprintf(stderr, "polling timeout");
            return ESP_ERR_TIMEOUT;
        }

        vTaskDelay(pdMS_TO_TICKS(poll_interval_ms));
    }
}

// puts into standby and returns back to standby if switch_to_rx_after_tx is not set.
// internally calls packet_to_bytestream() function
// polls for TxDone and clears irq flags
// verifies that acks match the current packet's src address
esp_err_t send_packet(packet *p, int switch_to_rx_after_tx)
{
    static uint8_t tx_buffer[255];
    uint8_t data = 0b10001001;
    esp_err_t ret = spi_burst_write_reg(sx_1278_spi, 0x01, &data, 1); // put in standby mode
    if (ret != ESP_OK)
    {
        fprintf(stderr, "couldnt put sx1278 in standby mode\n");
        return ret;
    }

    data = 0x00;
    ret = spi_burst_write_reg(sx_1278_spi, 0x0E, &data, 1); // set fifo base pointer
    ret = spi_burst_write_reg(sx_1278_spi, 0x0D, &data, 1); // set fifo pointer
    if (ret != ESP_OK)
    {
        fprintf(stderr, "couldnt set fifo address\n");
        return ret;
    }

    int packet_size = packet_to_bytestream(tx_buffer, sizeof(tx_buffer), p);
    if (packet_size == -1)
    {
        fprintf(stderr, "couldnt convert packet to bytesteam\n");
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t packet_size_byte = (uint8_t)(packet_size & 0xFF);

    ret = spi_burst_write_reg(sx_1278_spi, 0x22, &packet_size_byte, 1);        // write payload length
    ret = spi_burst_write_reg(sx_1278_spi, 0x00, tx_buffer, packet_size_byte); // write payload
    if (ret != ESP_OK)
    {
        fprintf(stderr, "couldnt write packet to tx fifo\n");
        return ret;
    }

    data = 0xFF;
    ret = spi_burst_write_reg(sx_1278_spi, 0x12, &data, 1); // clear irq flags
    if (ret != ESP_OK)
    {

        fprintf(stderr, "couldnt reset irq flags\n");
        return ret;
    }

    data = 0b10001011;
    ret = spi_burst_write_reg(sx_1278_spi, 0x01, &data, 1); // put in TX mode
    if (ret != ESP_OK)
    {

        fprintf(stderr, "couldnt switch to transmit mode\n");
        return ret;
    }

    ret = poll_for_irq_flag(3000, 3, (1 << 3), true); // poll until tx done flag is set
    if (ret != ESP_OK)
    {
        fprintf(stderr, "failed while polling for irq tx done flag\n");
        return ret;
    }

    data = switch_to_rx_after_tx ? 0b10001110 : 0b10001001;
    ret = spi_burst_write_reg(sx_1278_spi, 0x01, &data, 1); // put in next mode

    if (ret != ESP_OK)
    {
        fprintf(stderr, "couldnt put sx1278 innext mode after tx is complete\n");
        return ESP_ERROR_CHECK_WITHOUT_ABORT(0);
    }

    return ESP_OK;
}

// uses irq flags to check if rxdone is set but does not poll for the flag. for polling use poll_for_irq_flag
// resets irq
esp_err_t read_last_packet(packet *p)
{
    static uint8_t rx_buffer[255];

    const uint8_t irq_reg = 0x12;
    uint8_t data = 0;

    esp_err_t ret = spi_burst_read_reg(sx_1278_spi, irq_reg, &data, 1);
    if (ret != ESP_OK)
        return ret;
    uint8_t rx_done_mask = 0b01000000; // RxDone bit

    if (!(data & rx_done_mask))
    {
        data = 0xFF;
        spi_burst_write_reg(sx_1278_spi, irq_reg, &data, 1); // reset irq
        fprintf(stderr, "rx read not done\n");
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t rx_crc_mask = rx_done_mask >> 1; // PayloadCrcError bit
    if (data & rx_crc_mask)
    {
        data = 0xFF;
        spi_burst_write_reg(sx_1278_spi, irq_reg, &data, 1); // reset irq
        fprintf(stderr, "rx crc failed, discarding packet\n");
        return ESP_ERR_INVALID_CRC;
    }

    uint8_t n_packet_bytes = 0;
    ret = spi_burst_read_reg(sx_1278_spi, 0x13, &n_packet_bytes, 1); // read number of bytes

    uint8_t current_fifo_addr = 0;
    ret = spi_burst_read_reg(sx_1278_spi, 0x10, &current_fifo_addr, 1);
    if (ret != ESP_OK)
    {
        data = 0xFF;
        ret = spi_burst_write_reg(sx_1278_spi, irq_reg, &data, 1); // reset irq
        fprintf(stderr, "couldnt get rx fifo pointer, skipping packet\n");
        return ret;
    }

    ret = spi_burst_write_reg(sx_1278_spi, 0x0D, &current_fifo_addr, 1);
    if (ret != ESP_OK)
    {
        data = 0xFF;
        ret = spi_burst_write_reg(sx_1278_spi, irq_reg, &data, 1); // reset irq
        fprintf(stderr, "couldnt set rx fifo pointer, skipping packet\n");
        return ret;
    }

    ret = spi_burst_read_reg(sx_1278_spi, 0x00, rx_buffer, n_packet_bytes); // read payload
    if (ret != ESP_OK)
    {
        data = 0xFF;
        ret = spi_burst_write_reg(sx_1278_spi, irq_reg, &data, 1); // reset irq
        fprintf(stderr, "couldnt read rx fifo, skipping packet\n");
        return ret;
    }

    if (parse_packet(rx_buffer, p))
    {
        fprintf(stderr, "packet couldnt be parsed, discarding packet\n");
        data = 0xFF;
        ret = spi_burst_write_reg(sx_1278_spi, irq_reg, &data, 1); // reset irq
        memset(rx_buffer, 0x00, sizeof(rx_buffer));                // fill rx buffer with zeros

        return ESP_ERR_INVALID_STATE;
    }

    memset(rx_buffer, 0x00, sizeof(rx_buffer)); // fill rx buffer with zeros

    data = 0xFF;
    ret = spi_burst_write_reg(sx_1278_spi, irq_reg, &data, 1); // reset irq

    return ret;
}

esp_err_t initialize_sx_1278()
{
    uint8_t data = 0;
    esp_err_t ret = spi_burst_read_reg(sx_1278_spi, 0x42, &data, 1); // register version
    if (data != 0x12)
    {
        fprintf(stderr, "sx1278 register version is not valid");
        return ESP_ERR_INVALID_RESPONSE;
    }

    data = 0b10001000;
    ret = spi_burst_write_reg(sx_1278_spi, 0x01, &data, 1); // set sleep mode

    data = 0x00;
    ret = spi_burst_write_reg(sx_1278_spi, 0x0F, &data, 1); // set rx fifo addr
    ret = spi_burst_write_reg(sx_1278_spi, 0x0D, &data, 1); // set fifo ptr addr
    ret = spi_burst_write_reg(sx_1278_spi, 0x0E, &data, 1); // set tx fifo addr

    data = 0b10001001;
    ret = spi_burst_write_reg(sx_1278_spi, 0x01, &data, 1); // set stdby mode

    uint32_t frf = (((uint64_t)(lora_frequency_lf + lora_bandwidth_hz / 2)) << 19) / FXOSC;
    data = (frf >> 16) & 0xFF;
    ret = spi_burst_write_reg(sx_1278_spi, 0x06, &data, 1); // send frf big endian
    data = (frf >> 8) & 0xFF;
    ret = spi_burst_write_reg(sx_1278_spi, 0x07, &data, 1);
    data = (frf) & 0xFF;
    ret = spi_burst_write_reg(sx_1278_spi, 0x08, &data, 1);

    data = 0xFC;
    ret = spi_burst_write_reg(sx_1278_spi, 0x09, &data, 1); // power

    data = 0x0A;
    ret = spi_burst_write_reg(sx_1278_spi, 0x2B, &data, 1); // OCB

    data = 0x84;
    ret = spi_burst_write_reg(sx_1278_spi, 0x4D, &data, 1); // max power

    data = 0b10000010;
    ret = spi_burst_write_reg(sx_1278_spi, 0x1D, &data, 1); // 250kHz 4/5 coding explicit headers

    data = 0b11000100;
    ret = spi_burst_write_reg(sx_1278_spi, 0x1E, &data, 1); // 12 spreading factor, single packet mode , crc on, 0xFF symbtimeout

    data = 0xFF;
    ret = spi_burst_write_reg(sx_1278_spi, 0x1F, &data, 1); // 0xFF symbtimeout

    data = 0x00;
    ret = spi_burst_write_reg(sx_1278_spi, 0x20, &data, 1); // preamble length msb and lsb
    data = 0x08;
    ret = spi_burst_write_reg(sx_1278_spi, 0x21, &data, 1);

    data = SFD;
    ret = spi_burst_write_reg(sx_1278_spi, 0x39, &data, 1); // sync word
    // TODO dio pin configuration

    return ret;
}