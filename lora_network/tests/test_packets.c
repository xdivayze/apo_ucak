#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "packet.h"
#include <stdio.h>

int test_calculate_crc()
{
    uint8_t *data = malloc(8);
    uint16_t hand_calculated_crc = 0x97DF;
    memset(data, 0xFF, 8);
    uint16_t calculated_crc = calculate_crc(data, 8);

    if (calculated_crc != hand_calculated_crc)
    {
        fprintf(stderr, "calcalated crc:0x%2x do not match the hand calculated crc: 0x%2x\n", calculated_crc, hand_calculated_crc);
        free(data);
        return -1;
    }
    free(data);

    return 0;
}



int main()
{
    assert(test_calculate_crc() == 0);
    return 0;
}