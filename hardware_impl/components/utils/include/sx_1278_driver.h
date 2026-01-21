#pragma once

#include "esp_err.h"
#include "spi_utils.h"

#define MODE_LORA (1 << 7)
#define MODE_LF (1 << 3)
#define MODE_SLEEP (0b000 & 0xFF)
#define MODE_STDBY (0b001 & 0xFF)
#define MODE_TX (0b011 & 0xFF)
#define MODE_RX_SINGLE (0b110 & 0xFF)
#define MODE_RX_CONTINUOUS (0b101 & 0xFF)

#define MODE_FSK_RECEIVER (0b101 & 0xFF)

#define PHY_TIMEOUT_MSEC 3000

extern spi_device_handle_t sx_1278_spi;
esp_err_t sx1278_send_payload(uint8_t *buf, uint8_t len, int switch_to_rx_after_tx);

size_t calculate_channel_num();

esp_err_t sx1278_switch_mode(uint8_t mode_register);

esp_err_t sx_1278_get_op_mode(uint8_t *data);

esp_err_t initialize_sx_1278();

esp_err_t sx_1278_switch_to_nth_channel(size_t n);

esp_err_t sx_1278_set_spreading_factor(uint8_t sf);

esp_err_t sx1278_read_irq(uint8_t *data);

esp_err_t sx1278_read_last_payload(uint8_t *buf, size_t *len);

esp_err_t poll_for_irq_flag(size_t timeout_ms, size_t poll_interval_ms, uint8_t irq_and_mask, bool cleanup);

esp_err_t sx1278_clear_irq();
