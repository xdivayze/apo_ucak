#pragma once 
#include <stdint.h>
typedef struct
{
    uint8_t command;
    uint32_t params;
} ws_command_struct;