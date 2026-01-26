#pragma once
#include "esp_err.h"
#include <stdint.h>
#include "packet.h"
esp_err_t start_rx_loop(uint16_t host_addr);
esp_err_t read_last_packet(packet *p);
