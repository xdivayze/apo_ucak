#include "spi_utils.h"
#include "string.h"
#include "esp_log.h"
#include "esp_err.h"

#define TAG "spiutils"

esp_err_t spi_burst_write_reg(spi_device_handle_t spi, const uint8_t addr, const uint8_t *data, int len)
{

    esp_err_t ret = spi_device_acquire_bus(spi, portMAX_DELAY);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "couldnt acquire bus\n");
        return ret;
    }
    uint8_t *data_dma = heap_caps_malloc(len, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!data_dma)
    {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    memcpy(data_dma, data, len);
    if (len <= 0)
        return ESP_OK;
    
    uint8_t *cmd = heap_caps_malloc(1, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!cmd)
    {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    memset(cmd, addr | 0b10000000, 1);
    spi_transaction_t t0 = {0};
    t0.length = 8;
    t0.tx_buffer = cmd;
    t0.flags = SPI_TRANS_CS_KEEP_ACTIVE;

    spi_transaction_t t1 = {0};
    t1.length = len * 8;
    t1.tx_buffer = data_dma;

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
    if (cmd)
        free(cmd);
    if (data_dma)
        free(data_dma);
    spi_device_release_bus(spi);
    return ret;
}

esp_err_t spi_burst_read_reg(spi_device_handle_t spi, const uint8_t addr, uint8_t *data, int len)
{
    if (len <= 0)
        return ESP_OK;

    esp_err_t ret;

    uint8_t *dummy = heap_caps_malloc(len, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!dummy)
    {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    uint8_t *cmd = heap_caps_malloc(1, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!cmd)
    {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    memset(cmd, addr & 0b01111111, 1);

    spi_device_acquire_bus(spi, portMAX_DELAY);

    spi_transaction_t t0 = {0};
    t0.length = 8;
    t0.tx_buffer = cmd;
    t0.flags = SPI_TRANS_CS_KEEP_ACTIVE;

    ret = spi_device_transmit(spi, &t0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "t0: ret: %s\n", esp_err_to_name(ret));
        goto cleanup;
    }

    memset(dummy, 0x00, len);
    uint8_t *ret_data_dma = heap_caps_malloc(len, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!ret_data_dma)
    {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    spi_transaction_t t1 = {0};
    t1.length = len * 8;
    t1.tx_buffer = dummy;
    t1.rx_buffer = ret_data_dma;
    ret = spi_device_transmit(spi, &t1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "t1: ret: %s\n", esp_err_to_name(ret));
        goto cleanup;
    }

    memcpy(data, ret_data_dma, len);

cleanup:
    if (ret_data_dma)
        free(ret_data_dma);
    if (dummy)
        free(dummy);
    if (cmd)
        free(cmd);
    spi_device_release_bus(spi);
    return ret;
}