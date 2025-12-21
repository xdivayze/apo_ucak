#include "sx_1278_utils.h"
#include "spi_utils.h"
#include "stdlib.h"
#include "math.h"

#define FXOSC 32000000UL

#define lora_frequency_lf 410000000UL // datasheet table 32 for frequency bands
#define lora_bandwidth_khz 125        // table 12 for spreading factor-bandwidth relations
#define spreading_factor 12

esp_err_t initialize_sx_1278()
{
    uint8_t data = 0;
    burst_read_reg(sx_1278_spi, 0x42, &data, 1); // register version
    if (data != 0x12)
    {
        fprintf(stderr, "sx1278 register version is not valid");
        return ESP_ERR_INVALID_RESPONSE;
    }

    data = 0b10001000;
    burst_write_reg(sx_1278_spi, 0x01, &data, 1); // set sleep mode

    data = 0x00;
    burst_write_reg(sx_1278_spi, 0x0E, &data, 1); // set tx fifo addr
    burst_write_reg(sx_1278_spi, 0x0F, &data, 1); // set rx fifo addr
    burst_write_reg(sx_1278_spi, 0x0D, &data, 1); // set fifo ptr addr

    data = 0b10001001;
    burst_write_reg(sx_1278_spi, 0x01, &data, 1); // set stdby mode

    uint32_t frf = (((uint64_t)(lora_frequency_lf + lora_bandwidth_khz / 2)) << 19) / FXOSC;
    data = (frf >> 16) & 0xFF;
    burst_write_reg(sx_1278_spi, 0x06, &data, 1); // send frf big endian
    data = (frf >> 8) & 0xFF;
    burst_write_reg(sx_1278_spi, 0x07, &data, 1);
    data = (frf) & 0xFF;
    burst_write_reg(sx_1278_spi, 0x08, &data, 1);

    data = 0xFC;
    burst_write_reg(sx_1278_spi, 0x09, &data, 1); //power

    data = 0x0A;
    burst_write_reg(sx_1278_spi, 0x2B, &data, 1); //OCB

    data = 0x84;
    burst_write_reg(sx_1278_spi, 0x4D, &data, 1); //max power

    data = 0b10000010;
    burst_write_reg(sx_1278_spi, 0x1D, &data, 1); // 250kHz 4/5 coding explicit headers

    data = 0b11000100;
    burst_write_reg(sx_1278_spi, 0x1E, &data, 1); // 12 spreading factor, single packet mode , crc on, 0xFF symbtimeout

    data = 0xFF;
    burst_write_reg(sx_1278_spi, 0x1F, &data, 1); // 0xFF symbtimeout

    data = 0x00;
    burst_write_reg(sx_1278_spi, 0x20, &data, 1); // preamble length msb and lsb
    data = 0x08;
    burst_write_reg(sx_1278_spi, 0x21, &data, 1);

    data = SFD;
    burst_write_reg(sx_1278_spi, 0x39, &data, 1); //sync word

    return ESP_OK;
}