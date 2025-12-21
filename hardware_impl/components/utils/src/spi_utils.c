#include "spi_utils.h"

esp_err_t spi_burst_write_reg(spi_device_handle_t spi, const uint8_t addr, const uint8_t *data, int len)
{
    if (len <= 0)
        return ESP_OK;
    uint8_t cmd = addr | 0b10000000;
    spi_transaction_t t0 = {0};
    t0.length = 8;
    t0.tx_buffer = &cmd;
    t0.flags = SPI_TRANS_CS_KEEP_ACTIVE;

    spi_transaction_t t1 = {0};
    t1.length = len * 8;
    t1.tx_buffer = data;

    esp_err_t ret = spi_device_transmit(spi, &t0);
    if (ret != ESP_OK)
        return ret;
    ret = spi_device_transmit(spi, &t1);
    return ret;
}

esp_err_t spi_burst_read_reg(spi_device_handle_t spi, const uint8_t addr, uint8_t *data, int len)
{
    if (len <= 0)
        return ESP_OK;

    uint8_t cmd = addr & 0b01111111;
    spi_transaction_t t0 = {0};
    t0.length = 8;
    t0.tx_buffer = &cmd;
    t0.flags = SPI_TRANS_CS_KEEP_ACTIVE;

    esp_err_t ret = spi_device_transmit(spi, &t0);
    if (ret != ESP_OK)
        return ret;

    uint8_t dummy = 0x00;
    spi_transaction_t t1 = {0};
    t1.length = len * 8;
    t1.tx_buffer = &dummy;
    t1.rx_buffer = data;
    ret = spi_device_transmit(spi, &t1);
    return ret;
}