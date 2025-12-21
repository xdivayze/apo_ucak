#ifndef SPI_UTILS_H
#define SPI_UTILS_H

#include "driver/spi_master.h"

esp_err_t burst_read_reg(spi_device_handle_t spi, const uint8_t addr, const uint8_t *data, int len);
esp_err_t burst_write_reg(spi_device_handle_t spi, const uint8_t addr, const uint8_t *data, int len);
#endif