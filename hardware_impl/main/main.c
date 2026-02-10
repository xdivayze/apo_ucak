// PIN DEFINITIONS FOR QFN40
#include "spi_utils.h"
#include "sx_1278_utils.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "tx_tests.h"
#include "esp_timer.h"
#include "math.h"
#include "sx_1278_driver.h"
#include "rx_tests.h"
#include "sx_1278_config.h"
#include "sx_1278_rx_utils.h"
#include "rx_packet_handler.h"

#define ESP32C6

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

#define SX_SPI_CLOCK_SPEED 100000

spi_device_handle_t sx_1278_spi;
#define LORA_SPI_HOST SPI2_HOST

#define TAG "main v0.0.1"

static esp_err_t add_lora_device()
{
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

    esp_err_t ret = spi_bus_add_device(SPI2_HOST, &devcfg, &sx_1278_spi);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "couldnt add device to spi bus\n");
    }

    return ret;
}

static esp_err_t spi_init()
{

    spi_bus_config_t buscfg = {
        .miso_io_num = SX_MISO,
        .mosi_io_num = SX_MOSI,
        .sclk_io_num = SX_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,
    };

    esp_err_t ret = spi_bus_initialize(LORA_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "couldnt initialize spi bus\n");
    return ret;
}

// TODO ping pong initialization for device pairing
void app_main(void)
{

    ESP_LOGI(TAG, "init.. gg\n");

    ESP_ERROR_CHECK(spi_init());
    ESP_ERROR_CHECK(add_lora_device());

    ESP_ERROR_CHECK(initialize_sx_1278());

    ESP_LOGI(TAG, "sx1278 initialized\n");
    uint8_t data = 0;

    double rssi_vals[20] = {0.0};
    esp_err_t ret;
    size_t n = 0;

    ESP_ERROR_CHECK(sx_1278_set_spreading_factor(10));

    while (1)
    {
        // ret = sx_1278_get_channel_rssis(rssi_vals, &n);
        // if (ret != ESP_OK)
        // {
        //     ESP_LOGE(TAG, "error while reading rssi channels: %s\n", esp_err_to_name(ret));
        // }
        // for (size_t i = 0; i < n; i++)
        // {
        //     ESP_LOGI(TAG, "rssi at channel %zu/%zu: %.2f\n", i, n - 1, rssi_vals[i]);
        // }
        // ESP_ERROR_CHECK(initialize_sx_1278());

        ESP_ERROR_CHECK(spi_burst_read_reg(sx_1278_spi, 0x01, &data, 1));
        if (!(data >> 7))
            ESP_LOGE(TAG, "not in lora mode\n");
        ESP_LOGI(TAG, "sx1278 op mode: 0x%x", data);
        ESP_ERROR_CHECK(spi_burst_read_reg(sx_1278_spi, 0x1E, &data, 1));
        ESP_LOGI(TAG, "sx1278 sf: 0x%x", data);
#ifdef ESP32C3
        test_send_single_packet_expect_ack(3000);
        vTaskDelay(pdMS_TO_TICKS(1000));
#endif
#ifdef ESP32C6
        start_rx_loop();

#endif
    }

    spi_bus_remove_device(sx_1278_spi);
    spi_bus_free(LORA_SPI_HOST);
}
