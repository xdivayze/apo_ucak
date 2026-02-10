#pragma once
#include "esp_err.h"
#include <stdint.h>
#include "packet.h"
esp_err_t start_rx_loop();
esp_err_t read_last_packet(packet *p);
