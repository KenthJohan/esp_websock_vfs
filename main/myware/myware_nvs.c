#include <nvs_flash.h>
#include <esp_log.h>
#include <string.h>

#include "myware_nvs.h"

static uint32_t private_nvs;

#define LOG_FAIL(fname, e)          ESP_LOGW("Myware::NVS", "%s() failed, reason = %s", (fname), esp_err_to_name((e)));
#define LOG_BOOL(fname, key, value) ESP_LOGI("Myware::NVS", "%s(): %s: %s", (fname), (key), (value) ? "true" : "false");
#define LOG_U32(fname, key, value)  ESP_LOGI("Myware::NVS", "%s(): %s: %li", (fname), (key), (value));
#define LOG_U8(fname, key, value)   ESP_LOGI("Myware::NVS", "%s(): %s: %i", (fname), (key), (int)(value));
#define LOG_STR(fname, key, value)  ESP_LOGI("Myware::NVS", "%s(): %s: %s", (fname), (key), (value));

esp_err_t Myware_nvs_init()
{
	ESP_LOGI(__func__, "nvs_flash_init()");
	ESP_ERROR_CHECK(nvs_flash_init());
	esp_err_t e = nvs_open("storage", NVS_READWRITE, &private_nvs);
	if (e != ESP_OK) {
		LOG_FAIL("nvs_open", e);
		return e;
	}
	return e;
}

esp_err_t Myware_nvs_set_u32_verbose(char const *key, uint32_t value, bool ignore_if_exist)
{
	esp_err_t e;
	uint32_t value0 = 0;
	e = nvs_get_u32(private_nvs, key, &value0);
	if (e == ESP_OK) {
		ESP_LOGI(__func__, "NVS-get: storage: %s: %li", key, value0);
		if (ignore_if_exist) {
			return e;
		}
	}
	ESP_LOGI(__func__, "NVS-set: storage: %s: %li", key, value);
	e = nvs_set_u32(private_nvs, key, value);
	if (e != ESP_OK) {
		LOG_FAIL("nvs_set_u32", e);
		return e;
	}
	return e;
}

esp_err_t Myware_nvs_set_bool_verbose(char const *key, bool value, bool ignore_if_exist)
{
	esp_err_t e;
	uint8_t value0 = 0;
	e = nvs_get_u8(private_nvs, key, &value0);
	if (e == ESP_OK) {
		LOG_BOOL("nvs_get_u8", key, value0);
		if (ignore_if_exist) {
			return e;
		}
	}
	e = nvs_set_u8(private_nvs, key, value);
	if (e != ESP_OK) {
		LOG_FAIL("nvs_set_u8", e);
		return e;
	}
	LOG_BOOL("nvs_set_u8", key, value);
	return e;
}

esp_err_t Myware_nvs_set_str_verbose(char const *key, char const *str, bool ignore_if_exist)
{
	esp_err_t e;
	size_t len;
	char str0[64] = {0};
	len = sizeof(str0) - 1;
	e = nvs_get_str(private_nvs, key, str0, &len);
	if (e == ESP_OK) {
		LOG_STR("nvs_get_str", key, str0);
		if (ignore_if_exist) {
			return e;
		}
	}
	e = nvs_set_str(private_nvs, key, str);
	if (e != ESP_OK) {
		LOG_FAIL("nvs_set_str", e);
		return e;
	}
	LOG_STR("nvs_get_str", key, str);
	return e;
}

esp_err_t Myware_nvs_get_bool_verbose(char const *key, bool *out)
{
	esp_err_t e;
	uint8_t current = 0;
	e = nvs_get_u8(private_nvs, key, &current);
	if (e != ESP_OK) {
		LOG_FAIL("nvs_get_u8", e);
		return e;
	}
	(*out) = current ? true : false;
	LOG_BOOL("nvs_get_u8", key, (*out));
	return e;
}

esp_err_t Myware_nvs_get_u32_verbose(char const *key, uint32_t *out)
{
	esp_err_t e;
	uint32_t value;
	e = nvs_get_u32(private_nvs, key, &value);
	if (e != ESP_OK) {
		LOG_FAIL("nvs_get_u8", e);
		return e;
	}
	(*out) = value;
	LOG_U32("nvs_get_u32", key, (*out));
	return e;
}

esp_err_t Myware_nvs_get_str_verbose(char const *key, char *out, size_t len)
{
	esp_err_t e;
	char buf[64] = {0};
	size_t l = sizeof(buf) - 1;
	e = nvs_get_str(private_nvs, key, buf, &l);
	if (e != ESP_OK) {
		LOG_FAIL("nvs_get_str", e);
		return e;
	}
	LOG_STR("nvs_get_str", key, buf);
	if (len > l) {
		memcpy(out, buf, l);
		buf[l] = '\0';
	} else {
		ESP_LOGW(__func__, "Buffer too small");
		return ESP_ERR_NO_MEM;
	}
	return e;
}