#pragma once

#include "esp_err.h"
#include "spi_utils.h"



#define PHY_TIMEOUT_MSEC 3000


esp_err_t sx1278_send_payload(uint8_t *buf, uint8_t len, int switch_to_rx_after_tx);


esp_err_t sx1278_read_last_payload(uint8_t *buf, size_t *len);

esp_err_t poll_for_irq_flag(size_t timeout_ms, size_t poll_interval_ms, uint8_t irq_and_mask, bool cleanup);

