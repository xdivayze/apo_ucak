#pragma once
#include "command.h"
#include "cJSON.h"
#include "esp_http_client.h"

int wifi_init(const char* ssid, const char* pass);

int http_send_get(char* uri, size_t timeout_ms);
