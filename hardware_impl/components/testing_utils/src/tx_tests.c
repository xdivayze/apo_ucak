#include "tx_tests.h"
#include "packet.h"
#include "esp_random.h"
#include <string.h>
#include "sx_1278_utils.h"
#include "sx_1278_driver.h"

#include "esp_log.h"
#define TAG "TX_TEST"

esp_err_t test_send_single_packet_expect_ack(int timeout)
{
    uint8_t d_val[5] = {'a', 'm', 'u', 'm', 'u'};
    uint8_t *d = malloc(sizeof(d_val));
    memcpy(d, d_val, sizeof(d_val));
    
    uint8_t data = 0;
    packet *p = packet_constructor(0x123, 0x456, 0x7, 0x08, 5, d);

    esp_err_t ret = send_packet_ensure_ack(p, timeout);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "error occured: %s", esp_err_to_name(ret));
    }

    free_packet(p);
    sx1278_clear_irq();
    sx1278_switch_mode(MODE_LORA | MODE_SLEEP);
    return ret;
}

esp_err_t test_send_single()
{
    uint8_t d[5] = {'a', 'm', 'u', 'm', 'u'};

    esp_err_t ret = sx1278_send_payload(d, 5, false);
    return ret;
}

esp_err_t test_send_single_packet()
{
    uint8_t d[5] = {'a', 'm', 'u', 'm', 'u'};
    packet *p = packet_constructor(0x123, 0x456, 0x7, 0x08, 5, d);

    esp_err_t ret = sx_1278_send_packet(p, false);

    free_packet(p);
    return ret;
}