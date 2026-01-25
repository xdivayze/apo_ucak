#include "unity.h"
#include "sx_1278_config.h"
#include "spi_utils.h"
#include "esp_err.h"
#include "driver/gpio.h"

#define TAG "sx_1278 configurer test"

#define ESP32C3

#ifdef ESP32C6
#define SX_NSS GPIO_NUM_18
#define SX_SCK GPIO_NUM_14
#define SX_MISO GPIO_NUM_3
#define SX_MOSI GPIO_NUM_4
#endif

#ifdef ESP32C3
#define SX_NSS GPIO_NUM_3
#define SX_SCK GPIO_NUM_0
#define SX_MISO GPIO_NUM_1
#define SX_MOSI GPIO_NUM_2
#endif

#define SX_SPI_CLOCK_SPEED 10000

spi_device_handle_t sx_1278_spi;
#define LORA_SPI_HOST SPI2_HOST

void setup_lora_spi()
{

    spi_bus_config_t buscfg = {
        .miso_io_num = SX_MISO,
        .mosi_io_num = SX_MOSI,
        .sclk_io_num = SX_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(LORA_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = SX_SPI_CLOCK_SPEED,
        .mode = 0,
        .spics_io_num = SX_NSS,
        .queue_size = 4,
        .flags = 0,
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
    };


    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &sx_1278_spi));
}

void free_lora_spi()
{
    ESP_ERROR_CHECK(spi_bus_remove_device(sx_1278_spi));
    ESP_ERROR_CHECK(spi_bus_free(LORA_SPI_HOST));
}

TEST_CASE("setting sf works", "[sx_1278_config]")
{
    setup_lora_spi();

    ESP_ERROR_CHECK(initialize_sx_1278());

    ESP_ERROR_CHECK(sx_1278_set_spreading_factor(7));

    uint8_t data;
    ESP_ERROR_CHECK(spi_burst_read_reg(sx_1278_spi, 0x1E, &data, 1));
    TEST_ASSERT_EQUAL_UINT8(0x07, data >> 4);

    free_lora_spi();
}

TEST_CASE("setting bw works", "[sx_1278_config]")
{
    setup_lora_spi();

    ESP_ERROR_CHECK(initialize_sx_1278());

    ESP_ERROR_CHECK(sx1278_set_bandwidth(b62k5, cr4d5, true));

    uint8_t data;
    ESP_ERROR_CHECK(spi_burst_read_reg(sx_1278_spi, 0x1D, &data, 1));
    TEST_ASSERT_EQUAL_UINT8(0b01100010, data);

    free_lora_spi();
}
