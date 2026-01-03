#include "spi_utils.h"
#include "string.h"
#include "esp_log.h"
#include "esp_err.h"

#define TAG "spiutils"
esp_err_t spi_burst_write_reg(spi_device_handle_t spi, const uint8_t addr, const uint8_t *data, int len)
{
    if (len <= 0)
        return ESP_OK;
    esp_err_t ret = spi_device_acquire_bus(spi, portMAX_DELAY);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "couldnt acquire bus\n");
        return ret;
    }
    uint8_t cmd = addr | 0b10000000;
    spi_transaction_t t0 = {0};
    t0.length = 8;
    t0.tx_buffer = &cmd;
    t0.flags = SPI_TRANS_CS_KEEP_ACTIVE;

    spi_transaction_t t1 = {0};
    t1.length = len * 8;
    t1.tx_buffer = data;

    ret = spi_device_transmit(spi, &t0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "t0:%s \n", esp_err_to_name(ret));
        goto cleanup;
    }
    ret = spi_device_transmit(spi, &t1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "t1:%s \n", esp_err_to_name(ret));
        goto cleanup;
    }

cleanup:
    spi_device_release_bus(spi);
    return ret;
}

esp_err_t spi_read_reg1(spi_device_handle_t spi, uint8_t reg, uint8_t *out)
{
    spi_transaction_t t = {0};
    t.length = 16; // 8 bits addr + 8 bits dummy
    t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;

    t.tx_data[0] = reg & 0x7F; // read
    t.tx_data[1] = 0x00;       // dummy clocks

    esp_err_t ret = spi_device_transmit(spi, &t);
    if (ret != ESP_OK)
        return ret;

    *out = t.rx_data[1]; // first byte is garbage during address phase
    return ESP_OK;
}

esp_err_t spi_burst_read_reg(spi_device_handle_t spi, const uint8_t addr, uint8_t *data, int len)
{
    if (len <= 0)
        return ESP_OK;

    uint8_t dummy[len];

    spi_device_acquire_bus(spi, portMAX_DELAY);

    uint8_t cmd = addr & 0b01111111;
    spi_transaction_t t0 = {0};
    t0.length = 8;
    t0.tx_buffer = &cmd;
    t0.flags = SPI_TRANS_CS_KEEP_ACTIVE;

    esp_err_t ret = spi_device_transmit(spi, &t0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "t0: ret: %s\n", esp_err_to_name(ret));
        goto cleanup;
    }

    memset(dummy, 0x00, len);
    spi_transaction_t t1 = {0};
    t1.length = len * 8;
    t1.tx_buffer = dummy;
    t1.rx_buffer = data;
    ret = spi_device_transmit(spi, &t1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "t1: ret: %s\n", esp_err_to_name(ret));
        goto cleanup;
    }

cleanup:
    spi_device_release_bus(spi);
    return ret;
}