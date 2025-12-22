#include "sx_1278_utils.h"
#include "spi_utils.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "esp_err.h"
#include "esp_timer.h"

#define FXOSC 32000000UL

#define lora_frequency_lf 410000000UL // datasheet table 32 for frequency bands
#define lora_bandwidth_khz 125        // table 12 for spreading factor-bandwidth relations
#define spreading_factor 12

esp_err_t poll_for_irq_flag(size_t timeout_ms, size_t poll_interval_ms, uint8_t irq_and_mask)
{
    timeout_ms = (timeout_ms <= 0) ? 3000 : timeout_ms;
    poll_interval_ms = (poll_interval_ms <= 0) ? 2 : poll_interval_ms;

    const int64_t start = esp_timer_get_time();
    const int64_t timeout_us = (int64_t)timeout_ms * 1000;

    uint8_t irq = 0;
    esp_err_t ret;

    int64_t elapsed_us = 0;

    while (1)
    {
        ret = spi_burst_read_reg(sx_1278_spi, 0x12, &irq, 1);
        if (ret != ESP_OK)
        {
            fprintf(stderr, "couldnt read irq register\n");
            return ret;
        }
        if (irq & irq_and_mask)
        {
            return ESP_OK;
        }

        elapsed_us = esp_timer_get_time() - start;
        if (elapsed_us > timeout_us)
        {
            fprintf(stderr, "polling timeout");
            return ESP_ERR_TIMEOUT;
        }

        vTaskDelay(pdMS_TO_TICKS(timeout_ms));
    }
}

// puts into standby and returns back to standby. internally calls packet_to_bytestream() function
esp_err_t send_packet(packet *p)
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

    ret = spi_burst_write_reg(sx_1278_spi, 0x22, &packet_size_byte, 1);         // write payload length
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

    ret = poll_for_irq_flag(3000, 3, (1 << 3)); // poll until tx done flag is set
    if (ret != ESP_OK)
    {
        fprintf(stderr, "failed while polling for irq tx done flag\n");
        return ret;
    }

    data = 0xFF;
    spi_burst_write_reg(sx_1278_spi, 0x12, &data, 1); // clear irq flags

    data = 0b10001001;
    ret = spi_burst_write_reg(sx_1278_spi, 0x01, &data, 1); // put in standby mode

    if (ret != ESP_OK)
    {
        fprintf(stderr, "couldnt put sx1278 in standby mode after tx is complete\n");
        return ESP_ERROR_CHECK_WITHOUT_ABORT(0);
    }

    return ESP_OK;
}

esp_err_t read_last_packet(packet *p)
{
    static uint8_t rx_buffer[255];

    const uint8_t irq_reg = 0x12;
    uint8_t data = 0;

    esp_err_t ret = spi_burst_read_reg(sx_1278_spi, irq_reg, &data, 1);
    if (ret != ESP_OK)
        return ret;
    uint8_t rx_done_mask = 0b01000000;

    if (data & rx_done_mask)
    {
        data = 0xFF;
        spi_burst_write_reg(sx_1278_spi, irq_reg, &data, 1); // reset irq
        fprintf(stderr, "rx read not done\n");
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t rx_crc_mask = rx_done_mask >> 1;
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

    uint32_t frf = (((uint64_t)(lora_frequency_lf + lora_bandwidth_khz / 2)) << 19) / FXOSC;
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