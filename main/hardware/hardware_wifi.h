#pragma once
#include <stdbool.h>
#include <esp_err.h>

esp_err_t Hardware_wifi_start();
esp_err_t Hardware_wifi_disconnect();
esp_err_t Hardware_wifi_stop();
esp_err_t Hardware_wifi_connect(const char *ssid, const char *pass, int timeout_ms);

esp_err_t Hardware_wifi_print_ip(FILE *f);

esp_err_t Hardware_wifi_scanap(void);