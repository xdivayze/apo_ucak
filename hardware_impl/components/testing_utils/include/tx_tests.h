#pragma once
#include "esp_err.h"

esp_err_t test_send_single_packet();
esp_err_t test_send_single();
esp_err_t test_send_single_packet_expect_ack(int timeout);
esp_err_t test_send_burst();