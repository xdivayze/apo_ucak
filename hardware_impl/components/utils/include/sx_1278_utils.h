#ifndef SX_1278_UTILS_H
#define SX_1278_UTILS_H

#include "driver/spi_master.h"
#include "packet.h"

extern spi_device_handle_t sx_1278_spi;

extern size_t ack_timeout_msec;

void initialize_sx_1278_utils(spi_device_handle_t spi, size_t ack_timeout_msec, float lorawan_bandwidth_center);

esp_err_t initialize_sx_1278();

esp_err_t poll_for_irq_flag(size_t timeout_ms, size_t poll_interval_ms, uint8_t irq_and_mask, bool cleanup);

// send packet. does not concern itself with acks
esp_err_t send_packet(packet *p, int switch_to_rx_after_tx);

// read packet, dont send an ack
esp_err_t read_last_packet(packet *p_out);

// send len packets from p_buf and expect acks.
esp_err_t send_burst(packet **p_buf, const int len);

// read with successive acks to p_buf from BEGIN until DONE packet and write length to len
esp_err_t read_burst(packet **p_buf, int *len, int handshake_timeout, uint32_t host_addr);

esp_err_t sx_1278_get_channel_rssis(double *rssi_data, size_t *len);

esp_err_t sx_1278_set_spreading_factor(uint8_t sf);

esp_err_t sx_1278_switch_to_nth_channel(size_t n);

#endif