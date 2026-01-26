#ifndef SX_1278_UTILS_H
#define SX_1278_UTILS_H

#include "driver/spi_master.h"
#include "packet.h"



extern size_t ack_timeout_msec;




// send packet. does not concern itself with acks
esp_err_t sx_1278_send_packet(packet *p, int switch_to_rx_after_tx);

// read packet, dont send an ack
esp_err_t read_last_packet(packet *p_out);

// send len packets from p_buf and expect acks.
esp_err_t send_burst(packet **p_buf, const int len);

esp_err_t send_packet_ensure_ack(packet *p, int timeout, packet_types ack_type);
// read with successive acks to p_buf from BEGIN until DONE packet and write length to len
esp_err_t read_burst(packet **p_buf, int *len, int handshake_timeout, uint16_t host_addr);

esp_err_t sx_1278_get_channel_rssis(double *rssi_data, size_t *len);
esp_err_t sx1278_poll_and_read_packet(packet *rx_p, int timeout);

esp_err_t poll_for_irq_flag_no_timeout(size_t poll_interval_ms, uint8_t irq_and_mask, bool cleanup);

esp_err_t start_rx_loop(uint16_t host_addr);   
#endif