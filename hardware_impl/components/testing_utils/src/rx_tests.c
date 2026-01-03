#include "rx_tests.h"
#include "network_data_operations.h"
#include "sx_1278_utils.h"
#include "esp_err.h"
#include "esp_random.h"

#define BURST_RECEIVE_TEST_PACKET_COUNT 10;

esp_err_t test_receive_burst(int timeout)
{
    esp_err_t ret;
    packet **p_buf = malloc(sizeof(packet) * 20);
    if (!p_buf) return ESP_ERR_NO_MEM;
    int len = 0;
    ret = read_burst(p_buf, &len, 3000, esp_random());
    if (ret != ESP_OK)
        return ESP_OK;
    if (len != 10) return ESP_ERR_INVALID_STATE;
    
    return ESP_OK;
}

esp_err_t test_receive_single_packet(int timeout)
{
    esp_err_t ret;
    ret = poll_for_irq_flag(timeout, 5, 1 << 6, false);
    if (ret != ESP_OK)
        goto cleanup;
    packet *rx_p = malloc(sizeof(packet));
    ret = read_last_packet(rx_p);
    if (ret != ESP_OK)
        goto cleanup;

    if (rx_p)
        ret = ESP_OK;

cleanup:
    if (rx_p)
        free_packet(rx_p);
    return ret;
}