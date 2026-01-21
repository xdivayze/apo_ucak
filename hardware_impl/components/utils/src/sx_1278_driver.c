#include "sx_1278_driver.h"
#include "spi_utils.h"
#include "packet.h"
#include "esp_log.h"
#include <math.h>
#include "esp_timer.h"
#include <stdbool.h>

#define TAG "sx_1278_driver"


#define FXOSC 32000000UL

#define lora_frequency_lf 863000000 // datasheet table 32 for frequency bands
#define lora_frequency_hf 865000000
static uint32_t lora_bandwidth_hz = 125000; // table 12 for spreading factor-bandwidth relations
static uint8_t spreading_factor = 12;

size_t calculate_channel_num()
{
    return (size_t)floor((lora_frequency_hf - lora_frequency_lf - 2 * lora_bandwidth_hz) / lora_bandwidth_hz);
}
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

esp_err_t sx1278_read_irq(uint8_t *data)
{
    return spi_burst_read_reg(sx_1278_spi, 0x12, data, 1);
}

esp_err_t sx1278_clear_irq()
{
    uint8_t data = 0xFF;
    return spi_burst_write_reg(sx_1278_spi, 0x12, &data, 1);
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

esp_err_t sx1278_switch_mode(uint8_t mode_register)
{

    esp_err_t ret = spi_burst_write_reg(sx_1278_spi, 0x01, &mode_register, 1);

    return ret;
}

esp_err_t sx_1278_get_op_mode(uint8_t *data)
{
    return spi_burst_read_reg(sx_1278_spi, 0x01, data, 1);
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

esp_err_t sx_1278_switch_to_nth_channel(size_t n)
{

    if (n >= calculate_channel_num())
        return ESP_ERR_INVALID_ARG;
    uint64_t raw_freq = (uint64_t)(lora_frequency_lf + n * lora_bandwidth_hz + lora_bandwidth_hz / 2);
    uint32_t frf = ((raw_freq) << 19) / FXOSC;
    uint8_t data = (frf >> 16) & 0xFF;
    ESP_ERROR_CHECK(spi_burst_write_reg(sx_1278_spi, 0x06, &data, 1)); // send frf big endian
    data = (frf >> 8) & 0xFF;
    ESP_ERROR_CHECK(spi_burst_write_reg(sx_1278_spi, 0x07, &data, 1));
    data = (frf) & 0xFF;
    ESP_ERROR_CHECK(spi_burst_write_reg(sx_1278_spi, 0x08, &data, 1));

    return ESP_OK;
}

esp_err_t sx_1278_set_spreading_factor(uint8_t sf)
{

    if (sf < 6 || sf > 12)
        return ESP_ERR_INVALID_ARG;

    uint8_t data = 0b00000100;
    data |= sf << 4;
    esp_err_t ret = spi_burst_write_reg(sx_1278_spi, 0x1E, &data, 1);
    if (ret == ESP_OK)
        spreading_factor = sf;
    return ret;
}

// settings are done for 25mW = 14dBm
esp_err_t initialize_sx_1278()
{
    uint8_t data = 0;
    ESP_ERROR_CHECK(spi_burst_read_reg(sx_1278_spi, 0x42, &data, 1)); // register version
    if (data != 0x12)
    {
        ESP_LOGE(TAG, "sx1278 register version is not valid");
        return ESP_ERR_INVALID_RESPONSE;
    }

    data = 0b10000000;
    ESP_ERROR_CHECK(spi_burst_write_reg(sx_1278_spi, 0x01, &data, 1)); // set sleep mode

    data = 0x00;
    ESP_ERROR_CHECK(spi_burst_write_reg(sx_1278_spi, 0x0D, &data, 1)); // set fifo ptr addr
    ESP_ERROR_CHECK(spi_burst_write_reg(sx_1278_spi, 0x0F, &data, 1)); // set rx fifo base addr
    ESP_ERROR_CHECK(spi_burst_write_reg(sx_1278_spi, 0x0E, &data, 1)); // set tx fifo base baddr

    ESP_ERROR_CHECK(sx1278_switch_mode(MODE_LORA | MODE_STDBY));

    ESP_ERROR_CHECK(sx_1278_switch_to_nth_channel(0));

    data = 0b11111100;                                                 // Pout = 14dbM
    ESP_ERROR_CHECK(spi_burst_write_reg(sx_1278_spi, 0x09, &data, 1)); // power

    data = 0b00101011;                                                 // max current 100mA
    ESP_ERROR_CHECK(spi_burst_write_reg(sx_1278_spi, 0x2B, &data, 1)); // OCB

    data = 0x84;
    ESP_ERROR_CHECK(spi_burst_write_reg(sx_1278_spi, 0x4D, &data, 1)); // extra power mode not set

    data = 0b01110010;
    ESP_ERROR_CHECK(spi_burst_write_reg(sx_1278_spi, 0x1D, &data, 1)); // 125kHz 4/5 coding explicit headers

    ESP_ERROR_CHECK(sx_1278_set_spreading_factor(spreading_factor));

    data = 0xFF;
    ESP_ERROR_CHECK(spi_burst_write_reg(sx_1278_spi, 0x1F, &data, 1)); // 0xFF symbtimeout

    data = 0x00;
    ESP_ERROR_CHECK(spi_burst_write_reg(sx_1278_spi, 0x20, &data, 1)); // preamble length msb and lsb
    data = 0x08;
    ESP_ERROR_CHECK(spi_burst_write_reg(sx_1278_spi, 0x21, &data, 1));

    data = SFD;
    ESP_ERROR_CHECK(spi_burst_write_reg(sx_1278_spi, 0x39, &data, 1)); // sync word
    // TODO dio pin configuration

    return ESP_OK;
}
