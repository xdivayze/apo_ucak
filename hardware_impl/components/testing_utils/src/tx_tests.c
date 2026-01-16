#include "tx_tests.h"
#include "packet.h"
#include "esp_random.h"
#include <string.h>
#include "sx_1278_utils.h"
#include "sx_1278_driver.h"

#include "esp_log.h"
#define TAG "TX_TEST"
esp_err_t test_send_single()
{
    uint8_t d[5] = {'a', 'm', 'u', 'm','u'};    

    esp_err_t ret = sx1278_send_payload(d,5, false);
    return ret;
}

esp_err_t test_send_single_packet()
{
    uint8_t d[5] = {'a', 'm', 'u', 'm','u'};
    memset(d, 0x34, 5);
    packet *p = packet_constructor(0x12211221, 0x21122112, 0x1122, 0x00, 5, d);
    

    esp_err_t ret = sx_1278_send_packet(p, false);
    return ret;
}