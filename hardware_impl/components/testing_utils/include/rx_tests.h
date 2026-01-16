#pragma once
#include "esp_err.h"
esp_err_t test_receive_single_packet(int timeout);

esp_err_t test_receive_burst(int timeout);
esp_err_t test_receive_single(int timeout);