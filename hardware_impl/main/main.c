// PIN DEFINITIONS FOR QFN40
#include "spi_utils.h"
#include "sx_1278_utils.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "tx_tests.h"
#include "esp_timer.h"
#include "math.h"
#define SX_NSS GPIO_NUM_18
#define SX_SCK GPIO_NUM_14
#define SX_MISO GPIO_NUM_3
#define SX_MOSI GPIO_NUM_4

#define SX_SPI_CLOCK_SPEED 10000

spi_device_handle_t sx_1278_spi;
#define LORA_SPI_HOST SPI2_HOST

#define TAG "main"

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

    esp_err_t ret = spi_bus_initialize(LORA_SPI_HOST, &buscfg, SPI_DMA_DISABLED);
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "couldnt initialize spi bus\n");
    return ret;
}
//TODO RSSI LBT initialization when the two kiddos are close and same rf RSSI conditions apply for the two 
//TODO auto channel switching
// keep track of the bitrate and reduce packet overhead to 8-10 bytes
void app_main(void)
{

    ESP_LOGI(TAG, "init.. gg\n");

    ESP_ERROR_CHECK(spi_init());
    ESP_ERROR_CHECK(add_lora_device());

    ESP_ERROR_CHECK(initialize_sx_1278());
    ESP_LOGI(TAG, "sx1278 initialized\n");
    uint8_t data = 0;

    while (1)
    {
        ESP_ERROR_CHECK(spi_burst_read_reg(sx_1278_spi, 0x01, &data, 1));
        if (!(data >> 7))
            ESP_LOGE(TAG, "not in lora mode\n");
        ESP_LOGI(TAG, "sx1278 op mode: 0x%x", data);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
