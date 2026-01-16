#include "rx_tests.h"
#include "network_data_operations.h"
#include "sx_1278_utils.h"
#include "esp_err.h"
#include "esp_random.h"
#include "esp_log.h"
#include "spi_utils.h"
#define BURST_RECEIVE_TEST_PACKET_COUNT 10;

esp_err_t test_receive_burst(int timeout)
{
    esp_err_t ret;
    packet **p_buf = malloc(sizeof(packet) * 20);
    if (!p_buf)
        return ESP_ERR_NO_MEM;
    int len = 0;
    ret = read_burst(p_buf, &len, 3000, esp_random());
    if (ret != ESP_OK)
        return ESP_OK;
    if (len != 10)
        return ESP_ERR_INVALID_STATE;

    return ESP_OK;
}

#define TAG "rx_test"

esp_err_t test_receive_single_packet(int timeout)
{
    uint8_t data = 0;

    esp_err_t ret = poll_for_irq_flag(timeout, 5, 1 << 6, false);
    if (ret != ESP_OK)
    {
        spi_burst_read_reg(sx_1278_spi, 0x12, &data, 1);
        ESP_LOGE(TAG, "couldn't poll for packet received flag, got %x", data);
        return ret;
    }
    packet *rx_p = malloc(sizeof(packet));
    ret = read_last_packet(rx_p);
    char str[255] = "";
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "couldnt read last packet");
        goto cleanup;
    }

    if (rx_p)
        ret = ESP_OK;

    memcpy(str, rx_p->payload, rx_p->payload_length);
    str[rx_p->payload_length] = '\0';
    ESP_LOGI(TAG, "received data length %02X\t data src addr: %08x\ndata: %s", rx_p->payload_length, rx_p->src_address, str);

cleanup:
    data = 0xFF;
    ret = spi_burst_write_reg(sx_1278_spi, 0x12, &data, 1); // put into rx
    if (rx_p)
        free_packet(rx_p);
    return ret;
}