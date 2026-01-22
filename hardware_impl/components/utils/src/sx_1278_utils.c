#include "sx_1278_utils.h"
#include "spi_utils.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "esp_err.h"
#include "esp_timer.h"
#include <stdbool.h>
#include "esp_log.h"
#include "sx_1278_driver.h"

#define TAG "sx_1278_utils"

// read from BEGIN to END packets sending ACKs in between
// discards anything not intended for host address
// assumes p_buf has sizeof(packet*)*num bytes
esp_err_t read_burst(packet **p_buf, int *len, int handshake_timeout, uint32_t host_addr)
{
    int n = -1;
    uint8_t data = 0;
    packet *p = malloc(sizeof(packet));

    char *packet_print = malloc(2048);

    esp_err_t ret;
    uint32_t target_addr = 0;
    uint16_t ack_id = 0;
    bool end_reached = false;
    int repeat = (int)round(handshake_timeout / PHY_TIMEOUT_MSEC);
    for (int i = 0; i < repeat; i++)
    {
        ret = sx1278_poll_and_read_packet(p, PHY_TIMEOUT_MSEC);

        if (ret != ESP_OK)
            continue;
        if (check_packet_features(p, p->src_address, host_addr, p->ack_id, 0, PACKET_BEGIN)) // accept if destination address matches and is a valid handshake packet
        {
            target_addr = p->src_address; // set the source address
            ack_id = p->ack_id;           // set the ack id
            n = 1;
            ret = sx_1278_send_packet(ack_packet(target_addr, host_addr, ack_id, p->sequence_number), true);
            ESP_LOGI(TAG, "sending BEGIN ACK");
            if (ret != ESP_OK)
                continue;
            break;
        }
    }

    if (n != 1)
    {
        ret = ESP_ERR_TIMEOUT;
        goto cleanup;
    }

    while (!end_reached)
    {
        for (int i = 0; i < (repeat + 1); i++)
        {
            if (i == repeat)
            {
                ret = ESP_ERR_TIMEOUT;
                goto cleanup;
            }
            ret = sx1278_poll_and_read_packet(p, PHY_TIMEOUT_MSEC);
            if (ret != ESP_OK)
                continue;

            packet_description(p, packet_print);
            ESP_LOGI(TAG, "got packet: %s", packet_print);

            if (check_packet_features(p, target_addr, host_addr, ack_id, n, PACKET_DATA)) // check if nth data packet
            {
                ESP_LOGI(TAG, "sending DATA ACK %i", n);
                ret = sx_1278_send_packet(ack_packet(target_addr, host_addr, ack_id, n), true);
                if (ret != ESP_OK)
                    continue; // if ACK is not sent repeat everything

                p_buf[n-1] = malloc(sizeof(packet));
                memcpy(p_buf[n-1], p, sizeof(packet)); // only update buffer if ack is sent
                n++;
                break;
            }
            else if (check_packet_features(p, target_addr, host_addr, ack_id, UINT32_MAX, PACKET_END))
            {
                ESP_LOGI(TAG, "sending END ACK");
                ret = sx_1278_send_packet(ack_packet(target_addr, host_addr, ack_id, UINT32_MAX), false); // end packet continue at standby
                if (ret != ESP_OK)
                    continue; // if ACK is not sent repeat everything
                end_reached = true;
                break;
            }
        }
    }

    ret = ESP_OK;
    *len = n-1;

cleanup:

    sx1278_switch_mode((MODE_LORA | MODE_SLEEP));
    free_packet(p);
    free(packet_print);

    return ret;
}

esp_err_t sx1278_poll_and_read_packet(packet *rx_p, int timeout)
{
    esp_err_t ret;
    uint8_t data;
    ret = sx1278_switch_mode(MODE_LORA | MODE_RX_SINGLE);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "couldnt switch to rx single");
        goto cleanup;
    }

    ret = poll_for_irq_flag(timeout, 5, 1 << 6, false);
    if (ret != ESP_OK)
    {
        sx1278_read_irq(&data);
        ESP_LOGE(TAG, "couldn't poll for packet received flag, got %x. Retrying...", data);
    }

    ret = sx1278_switch_mode(MODE_LORA | MODE_STDBY);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "couldnt switch to standby");
        goto cleanup;
    }

    ret = read_last_packet(rx_p);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "error occured while reading the last packet");
        goto cleanup;
    }

    ret = ESP_OK;

cleanup:
    sx1278_clear_irq();
    sx1278_switch_mode(MODE_LORA | MODE_SLEEP);
    return ret;
}

// this one only sends packets in the array and waits for acks in between always include the BEGIN and END packets in the array
// consumes packets in p_buf
esp_err_t send_burst(packet **p_buf, const int len)
{
    packet *curr_packet;
    esp_err_t ret;
    uint8_t data = 0;
    packet_types ptype;

    char *packet_desc = malloc(2048);

    for (int i = 0; i < len; i++)
    {

        curr_packet = p_buf[i];

        if (i == 0)
            ptype = PACKET_BEGIN;
        else if (i == len - 1)
            ptype = PACKET_END;
        else
            ptype = PACKET_ACK;

        packet_description(curr_packet, packet_desc);

        ESP_LOGI(TAG, "sending %i th packet %s", i, packet_desc);
        ret = send_packet_ensure_ack(curr_packet, 4 * PHY_TIMEOUT_MSEC, ptype);
        if (ret != ESP_OK)
            goto cleanup;
        free(p_buf[i]);
    }
    ret = ESP_OK;
cleanup:
    return ret;
}

// puts into standby and returns back to standby if switch_to_rx_after_tx is not set.
// internally calls packet_to_bytestream() function
// polls for TxDone and clears irq flags
esp_err_t sx_1278_send_packet(packet *p, int switch_to_rx_after_tx)
{
    static uint8_t tx_buffer[255];

    int packet_size = packet_to_bytestream(tx_buffer, sizeof(tx_buffer), p);

    if (packet_size == -1)
    {
        ESP_LOGE(TAG, "couldnt convert packet to bytesteam\n");
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t packet_size_byte = (uint8_t)(packet_size & 0xFF);

    esp_err_t ret = sx1278_send_payload(tx_buffer, packet_size, switch_to_rx_after_tx);
    return ret;
}

// uses irq flags to check if rxdone is set but does not poll for the flag. for polling use poll_for_irq_flag
// resets irq
// assumes standby mode
// allocates packet payload
esp_err_t read_last_packet(packet *p)
{
    static uint8_t rx_buffer[255];
    size_t len = 0;
    esp_err_t ret = sx1278_read_last_payload(rx_buffer, &len);
    int packet_size = parse_packet(rx_buffer, p);
    if (packet_size == -1)
    {
        ESP_LOGE(TAG, "packet couldnt be parsed, discarding packet\n");

        ret = ESP_ERR_INVALID_STATE;
        goto cleanup;
    }

    ret = ESP_OK;

cleanup:
    memset(rx_buffer, 0x00, sizeof(rx_buffer)); // fill rx buffer with zeros
    sx1278_clear_irq();
    return ret;
}

#define RSSI_READ_DELAY_MS 20

static esp_err_t switch_to_fsk()
{
    uint8_t data = 0x00;
    esp_err_t ret;
    ret = sx1278_switch_mode((MODE_LORA | MODE_SLEEP));
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "couldnt set sleep mode on lora\n");
        return ret;
    }

    ret = sx1278_switch_mode(MODE_SLEEP);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "couldnt set sleep mode on fsk\n");
        return ret;
    }

    ret = sx_1278_get_op_mode(&data);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "couldnt read operation mode register");
        return ret;
    }
    if (data >> 7)
    {
        ESP_LOGE(TAG, "operational mode stuck at lora 0x%02x \n", data);
        return ret;
    }

    return ret;
}

// timeout / PHY_TIMEOUT used
esp_err_t send_packet_ensure_ack(packet *p, int timeout, packet_types ack_type)
{
    packet *rx_p = malloc(sizeof(packet));
    uint8_t data = 0;
    esp_err_t ret;
    int timeout_n = (int)ceilf((float)timeout / PHY_TIMEOUT_MSEC);
    // try again if ack timeout, wrong ack
    for (int i = 0; i < timeout_n; i++)
    {
        ret = sx_1278_send_packet(p, 1);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "error occured while sending the packet");
            goto cleanup;
        }

        ret = sx1278_poll_and_read_packet(rx_p, PHY_TIMEOUT_MSEC);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "error occured while reading packet. retrying...");
            continue;
        }

        if (!check_packet_features(rx_p, p->dest_address, p->src_address, p->ack_id, p->sequence_number, ack_type))
        {
            ESP_LOGE(TAG, "mismatched packet. retrying...");
            continue;
        }

        ret = ESP_OK;
        goto cleanup;
    }

    ret = ESP_ERR_TIMEOUT;

cleanup:
    sx1278_clear_irq();
    free_packet(rx_p);
    sx1278_switch_mode(MODE_LORA | MODE_SLEEP);
    return ret;
}

// switching to FSK/OOK mode discondifures lora. be sure te recall sx_1278_init() again
// rssi_data gets values in dBm
esp_err_t sx_1278_get_channel_rssis(double *rssi_data, size_t *len)
{

    esp_err_t ret = switch_to_fsk();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "couldnt switch to fsk mode\n");
        return ret;
    }

    uint8_t data;
    ret = sx1278_switch_mode(MODE_FSK_RECEIVER);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "couldnt set rx mode on fsk\n");
        return ret;
    }

    size_t channel_n = calculate_channel_num();
    double *temp_rssi_arr = malloc(channel_n * sizeof(double));
    uint8_t ret_data;

    for (size_t i = 0; i < channel_n; i++)
    {
        ret_data = 0;
        ret = sx_1278_switch_to_nth_channel(i);
        if (ret != ESP_OK)
        {
            if (i == channel_n - 1)
                ESP_LOGE(TAG, "couldnt switch to channel: %li\n", i);
            else
                ESP_LOGE(TAG, "couldnt switch to channel: %li , trying next one\n", i);

            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(RSSI_READ_DELAY_MS));

        ret = spi_burst_read_reg(sx_1278_spi, 0x11, &ret_data, 1);
        if (ret != ESP_OK)
        {
            if (i == channel_n - 1)
                ESP_LOGE(TAG, "couldnt read rssi on channel: %li.\n", i);
            else
                ESP_LOGE(TAG, "couldnt read rssi on channel: %li. switching to next channel\n", i);
            continue;
        }

        temp_rssi_arr[i] = -ret_data / 2.0;
    }

    *len = channel_n;
    memcpy(rssi_data, temp_rssi_arr, channel_n * sizeof(double));

    free(temp_rssi_arr);
    ret = sx1278_switch_mode(MODE_SLEEP | MODE_LORA);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "couldnt switch back to lora sleep mode\n");
    }
    return ret;
}
