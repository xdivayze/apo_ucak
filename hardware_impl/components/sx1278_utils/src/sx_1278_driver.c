#include "sx_1278_driver.h"
#include "spi_utils.h"
#include "packet.h"
#include "esp_log.h"
#include <math.h>
#include "esp_timer.h"
#include <stdbool.h>
#include "sx_1278_config.h"

#define TAG "sx_1278_driver"





//len is the callback length
esp_err_t sx1278_read_last_payload(uint8_t *buf, size_t *len)
{
    uint8_t data = 0;
    esp_err_t ret = sx1278_read_irq(&data);

    if (ret != ESP_OK)
        return ret;
    uint8_t rx_done_mask = 0b01000000; // RxDone bit

    if (!(data & rx_done_mask))
    {
        sx1278_clear_irq();

        ESP_LOGE(TAG, "rx read not done\n");
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t rx_crc_mask = rx_done_mask >> 1; // PayloadCrcError bit
    if (data & rx_crc_mask)
    {
        sx1278_clear_irq();

        ESP_LOGE(TAG, "rx crc failed, discarding packet\n");
        return ESP_ERR_INVALID_CRC;
    }

    uint8_t n_packet_bytes = 0;
    ret = spi_burst_read_reg(sx_1278_spi, 0x13, &n_packet_bytes, 1); // read number of bytes

    uint8_t current_fifo_addr = 0;
    ret = spi_burst_read_reg(sx_1278_spi, 0x10, &current_fifo_addr, 1);
    if (ret != ESP_OK)
    {
        sx1278_clear_irq();
        ESP_LOGE(TAG, "couldnt get rx fifo pointer, skipping packet\n");
        return ret;
    }

    ret = spi_burst_write_reg(sx_1278_spi, 0x0D, &current_fifo_addr, 1);
    if (ret != ESP_OK)
    {
        sx1278_clear_irq();

        ESP_LOGE(TAG, "couldnt set rx fifo pointer, skipping packet\n");
        return ret;
    }

    ret = spi_burst_read_reg(sx_1278_spi, 0x00, buf, n_packet_bytes); // read payload
    if (ret != ESP_OK)
    {
        sx1278_clear_irq();
        ESP_LOGE(TAG, "couldnt read rx fifo, skipping packet\n");
        return ret;
    }


    sx1278_clear_irq();

    *len = n_packet_bytes;
    return ESP_OK;
}


esp_err_t sx1278_send_payload(uint8_t *buf, uint8_t len, int switch_to_rx_after_tx)
{

    uint8_t data = 0x00;
    esp_err_t ret = sx1278_switch_mode((MODE_LORA | MODE_STDBY));
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "couldnt put sx1278 in standby mode\n");
        return ret;
    }

    data = 0x00; //TODO refactor fifo read/writes
    ret = spi_burst_read_reg(sx_1278_spi, 0x0E, &data, 1); // read fifo tx base pointer
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "couldnt set fifo address\n");
        return ret;
    }
    ret = spi_burst_write_reg(sx_1278_spi, 0x0D, &data, 1); // set fifo pointer
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "couldnt set fifo address\n");
        return ret;
    }

    ret = spi_burst_write_reg(sx_1278_spi, 0x22, &len, 1); // write payload length
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "couldnt write packet length\n");
        return ret;
    }
    ret = spi_burst_write_reg(sx_1278_spi, 0x00, buf, len); // write payload
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "couldnt write packet to tx fifo\n");
        return ret;
    }

    data = 0xFF;
    ret = spi_burst_write_reg(sx_1278_spi, 0x12, &data, 1); // clear irq flags
    if (ret != ESP_OK)
    {

        ESP_LOGE(TAG, "couldnt reset irq flags\n");
        return ret;
    }

    ret = sx1278_switch_mode((MODE_LORA | MODE_TX));
    if (ret != ESP_OK)
    {

        ESP_LOGE(TAG, "couldnt switch to transmit mode\n");
        return ret;
    }

    ret = poll_for_irq_flag(3000, 3, (1 << 3), true); // poll until tx done flag is set
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "failed while polling for irq tx done flag\n");
        return ret;
    }

    data = switch_to_rx_after_tx ? MODE_RX_CONTINUOUS : MODE_SLEEP;
    ret = sx1278_switch_mode((MODE_LORA | data));

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "couldnt put sx1278 in next mode after tx is complete\n");
        return ESP_ERROR_CHECK_WITHOUT_ABORT(0);
    }

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
            ESP_LOGE(TAG, "couldnt read irq register\n");
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
            ESP_LOGE(TAG, "polling timeout");
            return ESP_ERR_TIMEOUT;
        }

        vTaskDelay(pdMS_TO_TICKS(poll_interval_ms));
    }
}

