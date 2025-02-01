#pragma once
#include <stdbool.h>
#include <esp_err.h>

esp_err_t Hardware_wifi_start();

esp_err_t Hardware_wifi_join(const char *ssid, const char *pass, int timeout_ms);

void Hardware_print_ip();