#pragma once

#include "esp_websocket_client.h"
typedef struct
{
    uint8_t command;
    uint32_t params;
} ws_command_struct;
void ws_init(esp_websocket_client_handle_t *ws_client);

int ws_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
