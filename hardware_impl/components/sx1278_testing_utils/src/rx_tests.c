#include "rx_tests.h"
#include "network_data_operations.h"
#include "sx_1278_utils.h"
#include "esp_err.h"
#include "esp_random.h"
#include "esp_log.h"
#include "spi_utils.h"
#include <stdbool.h>
#include "sx_1278_driver.h"
#include <math.h>
#include "esp_timer.h"
#include "sx_1278_config.h"

#define HOST_ADDR 0x1222
#define TARGET_ADDR 0x2111

#define TAG "rx_test"

// TODO burst receive test util where receiver only sends ack on the second receive

esp_err_t test_receive_burst(int timeout)
{
    int64_t t0 = esp_timer_get_time();
    int64_t t1 = 0;
    esp_err_t ret;
    packet **p_buf = malloc(sizeof(packet *) * 20);
    if (!p_buf)
        return ESP_ERR_NO_MEM;
    int len = 0;
    uint8_t *res_str = malloc(256);

    ret = read_burst(p_buf, &len, timeout, HOST_ADDR);
    if (ret != ESP_OK)
    {
        goto cleanup;
    }
    t1 = esp_timer_get_time();

    ESP_LOGI(TAG, "received %i packets in %.4f seconds.", len, (t1 - t0) / ((float)1e6));
    packet_array_to_data(p_buf, res_str, len);

    ESP_LOGI(TAG, "received data: %s", res_str);

cleanup:
    free(p_buf);
    free(res_str);
    return ret;
}

esp_err_t test_receive_single(int timeout)
{
    esp_err_t ret = sx1278_switch_mode(MODE_LORA | MODE_RX_SINGLE);
    uint8_t data;
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "couldnt switch to rx cont");
        return ret;
    }

    ret = poll_for_irq_flag(timeout, 5, 1 << 6, false);
    if (ret != ESP_OK)
    {
        spi_burst_read_reg(sx_1278_spi, 0x12, &data, 1);
        ESP_LOGE(TAG, "couldn't poll for packet received flag, got %x", data);
        return ret;
    }

    sx1278_switch_mode(MODE_LORA | MODE_STDBY);

    uint8_t rx_buf[255] = {0};
    size_t len = 0;
    ret = sx1278_read_last_payload(rx_buf, &len);

    log_hex(TAG, rx_buf, len);

    return ret;
}

esp_err_t test_receive_single_packet_send_ack(int timeout)
{
    uint8_t data = 0;

    packet *rx_p = malloc(sizeof(packet));
    rx_p->payload = NULL;
    char str[255];
    esp_err_t ret = sx1278_poll_and_read_packet(rx_p, timeout);
    packet *ack = NULL;
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "error occured while polling and reading packet: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = sx_1278_send_packet(ack_packet(rx_p->src_address, rx_p->dest_address, rx_p->ack_id, rx_p->sequence_number), 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "error occured while sending ack packet");
        goto cleanup;
    }

    memcpy(str, rx_p->payload, rx_p->payload_length);
    str[rx_p->payload_length] = '\0';

    ESP_LOGI(TAG, "received dest addr: 0x%08x, src addr: 0x%08x, ack id: 0x%04x, sequence: 0x%08x", rx_p->dest_address, rx_p->src_address, rx_p->ack_id, rx_p->sequence_number);
    ESP_LOGI(TAG, "received data length: %02X, data: %s", rx_p->payload_length, str);

    ret = ESP_OK;

cleanup:
    sx1278_clear_irq();
    free_packet(rx_p);
    if (ack)
        free_packet(ack);
    return ret;
}

esp_err_t test_receive_single_packet(int timeout)
{
    uint8_t data = 0;

    packet *rx_p = malloc(sizeof(packet));
    char str[255];
    esp_err_t ret = sx1278_poll_and_read_packet(rx_p, timeout);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "error occured while polling and reading packet: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    memcpy(str, rx_p->payload, rx_p->payload_length);
    str[rx_p->payload_length] = '\0';

    ESP_LOGI(TAG, "received dest addr: 0x%08x, src addr: 0x%08x, ack id: 0x%04x, sequence: 0x%08x", rx_p->dest_address, rx_p->src_address, rx_p->ack_id, rx_p->sequence_number);
    ESP_LOGI(TAG, "received data length: %02X, data: %s", rx_p->payload_length, str);

cleanup:
    sx1278_clear_irq();
    free_packet(rx_p);
    return ret;
}