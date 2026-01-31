#include <assert.h>
#include "network_data_operations.h"
#include <string.h>
#include "rx_packet_handler.h"
#include <stdio.h>
#include <math.h>

#define TEST_STRING "amumumumu sejuani"

#define COMMAND_PAYLOAD_STR "rammus"

static void log_info(char *str)
{
    printf("%s\n", str);
}

static void command_callback(packet *rx_p)
{
    free_packet(rx_p);
}

static void packet_end_callback(packet **p_arr, int n)
{
    uint8_t *data_arr = malloc(256);
    packet_array_to_data(p_arr, data_arr, n);
    assert(strcmp(data_arr, TEST_STRING) == 0);
    free(data_arr);
}

void test_handler_single_array_all_capture_with_redundancy()
{
    log_info("starting single array all capture with redundancy test");
    char *data_str = TEST_STRING;
    size_t data_len = strlen(data_str) + 1;
    uint8_t *data = malloc(data_len);
    uint16_t daddr = 0xFF00;
    uint16_t saddr = 0x00FF;

    configure_rx_packet_handler(command_callback, packet_end_callback, daddr, saddr);

    uint8_t data_str2[256];

    memcpy(data, data_str, data_len);

    size_t npackets = (size_t)ceil((double)data_len / payload_length_max) + 2;
    packet **p_arr = malloc(sizeof(packet *) * npackets);
    int ret;
    if (data_to_packet_array(p_arr, data, data_len, daddr, saddr, 0x01, true))
    {
        ret = -1;
        goto cleanup;
    }

    for (int i = 0; i < npackets; i++)
    {
        rx_packet_handler(p_arr[i]);

        assert(rx_packet_handler(p_arr[i]) == PACKET_REDUNDANT);
    }

cleanup:
    free(data);
    free(p_arr);
}

void test_handler_single_array_all_capture_no_redundancy()
{
    log_info("starting single array no redundancy all capture test");
    char *data_str = TEST_STRING;
    size_t data_len = strlen(data_str) + 1;
    uint8_t *data = malloc(data_len);
    uint16_t daddr = 0xFF00;
    uint16_t saddr = 0x00FF;

    configure_rx_packet_handler(command_callback, packet_end_callback, daddr, saddr);

    uint8_t data_str2[256];

    memcpy(data, data_str, data_len);

    size_t npackets = (size_t)ceil((double)data_len / payload_length_max) + 2;
    packet **p_arr = malloc(sizeof(packet *) * npackets);
    int ret;
    if (data_to_packet_array(p_arr, data, data_len, daddr, saddr, 0x01, true))
    {
        ret = -1;
        goto cleanup;
    }

    for (int i = 0; i < npackets; i++)
    {
        rx_packet_handler(p_arr[i]);
    }

cleanup:
    free(data);
    free(p_arr);
}

int main()
{
    test_handler_single_array_all_capture_no_redundancy();
    test_handler_single_array_all_capture_with_redundancy();
    return 0;
}