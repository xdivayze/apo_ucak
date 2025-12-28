#include "ws_utils.h"
#include "cJSON.h"
#include <stdint.h>
#include <stdlib.h>
static esp_websocket_client_handle_t *client = NULL;

typedef struct
{
    uint8_t command;
    uint32_t params;
} ws_command_struct;

static int json_parser(ws_command_struct *cmd, const char *data)
{
    int ret = -1;
    cJSON *root = cJSON_Parse(data);
    if (root == NULL)
    {
        const char *err = cJSON_GetErrorPtr();
        fprintf(stderr, "json parser returned %s\n", err);
        goto cleanup;
    }

    cJSON *command = cJSON_GetObjectItem(root, "command");
    if (cJSON_IsNumber(command))
    {
        int val = command->valueint;
        if ((val <= 0xFF) && (val >= 0))
        {
            cmd->command = (uint8_t)(val & 0xFF);
        }
        else
        {
            fprintf(stderr, "command not a single byte\n");
            goto cleanup;
        }
    }
    else
    {
        fprintf(stderr, "command not a number\n");
        goto cleanup;
    }

    cJSON *params = cJSON_GetObjectItem(root, "data");
    if (cJSON_IsNumber(params))
    {
        int dval = params->valueint;
        if ((dval <= UINT32_MAX) && (dval >= 0))
        {
            cmd->params = (uint32_t)(dval & 0xFFFFFFFF);
        }
        else
        {
            fprintf(stderr, "params not a 32 bit unsigned integer\n");
            goto cleanup;
        }
    }
    else
    {
        fprintf(stderr, "data not a number\n");
        goto cleanup;
    }

    ret = 0;

cleanup:
    if (root)
        cJSON_Delete(root);

    return ret;
}

void ws_init(esp_websocket_client_handle_t *ws_client)
{
    client = ws_client;
}

static ws_command_struct *ws_data_event_handler(esp_websocket_event_data_t *d)
{
    ws_command_struct *ws_cmd = malloc(sizeof(ws_command_struct));
    ws_command_struct *ret = NULL;
    if (!ws_cmd)
    {
        fprintf(stderr, "no mem\n");
        return ret;
    }

    char *buf = malloc(d->data_len + 1);
    if (!buf)
    {
        fprintf(stderr, "no mem\n");
        goto cleanup;
    }
    memcpy(buf, d->data_ptr, d->data_len);
    memset(&(buf[d->data_len]), '\0', 1);

    if (json_parser(ws_cmd, buf))
    {
        fprintf(stderr, "json couldnt be parsed\n");
        goto cleanup;
    }

    ret = malloc(sizeof(ws_command_struct));
    memcpy(ret, ws_cmd, sizeof(ws_command_struct));

cleanup:
    if (ws_cmd)
        free(ws_cmd);
    if (buf)
        free(buf);
    return ret;
}

int ws_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ws_command_struct *callback_ws_cmd = handler_args;
    esp_websocket_event_data_t *d = event_data;

    int ret = -1;

    switch (event_id)
    {
    case WEBSOCKET_EVENT_DATA: // extract command and parameters, write to buffer in handler_args
        ws_command_struct *ws_cmd = ws_data_event_handler(d);
        if (!ws_cmd)
        {
            fprintf(stderr, "ws data event handler returned NULL\n");
            return -1;
        }

        memcpy(callback_ws_cmd, ws_cmd, sizeof(ws_command_struct));
        free(ws_cmd);
        break;
    default:
        return -1;
        break;
    }
    return 0;
}
