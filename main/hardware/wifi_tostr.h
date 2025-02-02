#pragma once

#include <esp_wifi.h>

char const *wifi_event_tostr(wifi_event_t event);
char const *wifi_auth_mode_str(wifi_auth_mode_t authmode);
char const *wifi_cipher_type_str(wifi_cipher_type_t type);
