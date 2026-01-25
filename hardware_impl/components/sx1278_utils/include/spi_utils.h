#ifndef SPI_UTILS_H
#define SPI_UTILS_H

#include "driver/spi_master.h"
#include <stdint.h>
#include <stdlib.h>

esp_err_t spi_burst_read_reg(spi_device_handle_t spi, const uint8_t addr, uint8_t *data, int len);
esp_err_t spi_burst_write_reg(spi_device_handle_t spi, const uint8_t addr, const uint8_t *data, int len);
void log_hex(char* TAG, uint8_t *arr, size_t len);
#endif