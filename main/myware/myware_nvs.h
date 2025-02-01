#pragma once

#include <esp_err.h>
#include <stdint.h>
#include <stdbool.h>

esp_err_t Myware_nvs_init();
esp_err_t Myware_nvs_set_u32_verbose(char const *key, uint32_t value, bool ignore_if_exist);
esp_err_t Myware_nvs_set_bool_verbose(char const *key, bool value, bool ignore_if_exist);
esp_err_t Myware_nvs_set_str_verbose(char const *key, char const *str, bool ignore_if_exist);
esp_err_t Myware_nvs_get_bool_verbose(char const *key, bool *value);
esp_err_t Myware_nvs_get_str_verbose(char const *key, char *str, size_t len);
esp_err_t Myware_nvs_get_u32_verbose(char const *key, uint32_t *value);