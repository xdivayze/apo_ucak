#include "tx_tests.h"
#include "packet.h"
#include "esp_random.h"
#include <string.h>
#include "sx_1278_utils.h"
esp_err_t send_single_packet()
{
    uint8_t d[5] = {0};
    memset(d, 0x34, 5);
    packet *p = packet_constructor(esp_random(), esp_random(), esp_random(), 5, 0, d);
    esp_err_t ret = send_packet(p, false);
    return ret;
}