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
// TODO add adjustable size vector to temporarily store the packets then return
esp_err_t read_burst(packet **p_buf, int *len, int handshake_timeout, uint32_t host_addr)
{
    int n = -1;
    uint8_t data = 0;
    packet *p = malloc(sizeof(packet));
    if (!p)
        return ESP_ERR_NO_MEM;

    esp_err_t ret = sx1278_switch_mode((MODE_LORA | MODE_RX_CONTINUOUS));
    uint32_t target_addr = 0;
    uint16_t ack_id = 0;
    int repeat = (int)round(handshake_timeout / 2000.0);
    for (int i = 0; i < repeat; i++)
    {

        ret = poll_for_irq_flag(2000, 2, 1 << 6, false); // poll until packet received
        if (ret != ESP_OK)
            continue;

        ret = sx1278_switch_mode((MODE_LORA | MODE_STDBY));
        ret = read_last_packet(p);
        if (ret != ESP_OK)
            continue;
        if (check_packet_features(p, p->src_address, p->dest_address, p->ack_id, 0, PACKET_BEGIN)) // accept if destination address matches and is a valid handshake packet
        {
            target_addr = p->src_address; // set the source address
            ack_id = p->ack_id;           // set the ack id
            n = 0;
            ret = sx_1278_send_packet(ack_packet(target_addr, host_addr, ack_id, p->sequence_number), true);
            if (ret != ESP_OK)
                continue;
            break;
        }
        ret = sx1278_switch_mode((MODE_LORA | MODE_RX_CONTINUOUS));
    }

    if (n != 0)
    {
        ret = sx1278_switch_mode((MODE_LORA | MODE_STDBY));
        free_packet(p);
        return ESP_ERR_TIMEOUT;
    }
    bool end_reached = false;
    while (!end_reached)
    {
        for (int i = 0; i < (repeat + 1); i++)
        {
            if (i == repeat)
            {
                free_packet(p);
                return ESP_ERR_TIMEOUT;
            }
            ret = sx1278_switch_mode((MODE_LORA | MODE_RX_CONTINUOUS));
            ret = poll_for_irq_flag(2000, 2, 1 << 6, false);
            if (ret != ESP_OK)
                continue;
            ret = sx1278_switch_mode((MODE_LORA | MODE_STDBY));
            ret = read_last_packet(p);
            if (ret != ESP_OK)
                continue;

            if (check_packet_features(p, target_addr, host_addr, ack_id, n, PACKET_DATA)) // check if nth data packet
            {
                ret = sx_1278_send_packet(ack_packet(target_addr, host_addr, ack_id, n), true);
                if (ret != ESP_OK)
                    continue;                        // if ACK is not sent repeat everything
                memcpy(p_buf[n], p, sizeof(packet)); // only update buffer if ack is sent
                n++;
                break;
            }
            else if (check_packet_features(p, target_addr, host_addr, ack_id, UINT32_MAX, PACKET_END))
            {
                ret = sx_1278_send_packet(ack_packet(target_addr, host_addr, ack_id, UINT32_MAX), false); // end packet continue at standby
                if (ret != ESP_OK)
                    continue; // if ACK is not sent repeat everything
                end_reached = true;
                break;
            }
        }
    }
    free_packet(p);

    *len = n;
    return ESP_OK;
}

//timeout / PHY_TIMEOUT used
esp_err_t send_packet_ensure_ack(packet *p, int timeout)
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

        if (!check_packet_features(rx_p, p->dest_address, p->src_address, p->ack_id, p->sequence_number, PACKET_ACK))
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
esp_err_t send_burst(packet **p_buf, const int len)
{
    packet *curr_packet;
    esp_err_t ret;
    uint8_t data = 0;
    packet *rx_packet = malloc(sizeof(packet));
    for (int i = 0; i < len; i++)
    {
        while (1)
        {
            memset(rx_packet, 0, sizeof(*rx_packet));
            curr_packet = p_buf[i];
            ret = sx_1278_send_packet(curr_packet, 1);

            if (ret == ESP_ERROR_CHECK_WITHOUT_ABORT(0))
            {
                data = 0b10001110;
                ret = spi_burst_write_reg(sx_1278_spi, 0x01, &data, 1);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "retry failed when setting sx1278 to rx mode\n");
                    free_packet(rx_packet);
                    return ret;
                }
            }
            else if (ret != ESP_OK)
            {
                free_packet(rx_packet);
                return ret;
            }

            ret = poll_for_irq_flag(2000, 2, 1 << 6, false); // do not clear irq flags as read_last_packet needs them
            if (ret != ESP_OK)                               // if polling resulted in rxdone flag not being set retry
                continue;

            ret = read_last_packet(rx_packet);
            if (ret != ESP_OK)
            { // retry if ack packet integrity cannot be verified
                ESP_LOGE(TAG, "ack packet failed to be read, retrying\n");
                continue;
            }
            if (check_packet_features(rx_packet, curr_packet->dest_address, curr_packet->src_address, curr_packet->ack_id, curr_packet->sequence_number, PACKET_ACK))
            { // ith ACK packet
                break;
            }
            else
                continue; // retry if not ack packet
        }
    }
    free_packet(rx_packet);
    return ESP_OK;
}

// puts into standby and returns back to standby if switch_to_rx_after_tx is not set.
// internally calls packet_to_bytestream() function
// polls for TxDone and clears irq flags
// verifies that acks match the current packet's src address
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
