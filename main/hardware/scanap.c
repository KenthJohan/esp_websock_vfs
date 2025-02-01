/* Scan Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
    This example shows how to scan for available set of APs.
*/
#include "scanap.h"
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_event.h>

#define DEFAULT_SCAN_LIST_SIZE 20

#define FMT_AP_HEADER "%-40s %5s %5s %-20s %-10s %-10s"
#define FMT_AP_ROW    "%-40s %5i %5i %-20s %-10s %-10s"

static char const *wifi_auth_mode_str(wifi_auth_mode_t authmode)
{
	switch (authmode) {
	case WIFI_AUTH_OPEN:
		return "OPEN";
	case WIFI_AUTH_OWE:
		return "OWE";
	case WIFI_AUTH_WEP:
		return "WEP";
	case WIFI_AUTH_WPA_PSK:
		return "WPA_PSK";
	case WIFI_AUTH_WPA2_PSK:
		return "WPA2_PSK";
	case WIFI_AUTH_WPA_WPA2_PSK:
		return "WPA_WPA2_PSK";
	case WIFI_AUTH_ENTERPRISE:
		return "ENTERPRISE";
	case WIFI_AUTH_WPA3_PSK:
		return "WPA3_PSK";
	case WIFI_AUTH_WPA2_WPA3_PSK:
		return "WPA2_WPA3_PSK";
	case WIFI_AUTH_WPA3_ENT_192:
		return "WPA3_ENT_192";
	default:
		return "UNKNOWN";
	}
}

static char const *wifi_cipher_type_str(wifi_cipher_type_t type)
{
	switch (type) {
	case WIFI_CIPHER_TYPE_NONE:
		return "NONE";
	case WIFI_CIPHER_TYPE_WEP40:
		return "WEP40";
	case WIFI_CIPHER_TYPE_WEP104:
		return "WEP104";
	case WIFI_CIPHER_TYPE_TKIP:
		return "TKIP";
	case WIFI_CIPHER_TYPE_CCMP:
		return "CCMP";
	case WIFI_CIPHER_TYPE_TKIP_CCMP:
		return "TKIP_CCMP";
	case WIFI_CIPHER_TYPE_AES_CMAC128:
		return "AES_CMAC128";
	case WIFI_CIPHER_TYPE_SMS4:
		return "SMS4";
	case WIFI_CIPHER_TYPE_GCMP:
		return "GCMP";
	case WIFI_CIPHER_TYPE_GCMP256:
		return "GCMP256";
	default:
		return "UNKNOWN";
	}
}

esp_err_t scanap(void)
{
	esp_err_t e;
	uint16_t number = DEFAULT_SCAN_LIST_SIZE;
	wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
	uint16_t ap_count = 0;
	memset(ap_info, 0, sizeof(ap_info));
	e = esp_wifi_scan_start(NULL, true);
	if (e != ESP_OK) {
		ESP_LOGW(__func__, "esp_wifi_scan_start() failed, reason = %s", esp_err_to_name(e));
		return e;
	}
	e = esp_wifi_scan_get_ap_num(&ap_count);
	if (e != ESP_OK) {
		ESP_LOGW(__func__, "esp_wifi_scan_get_ap_num() failed, reason = %s", esp_err_to_name(e));
		return e;
	}
	e = esp_wifi_scan_get_ap_records(&number, ap_info);
	if (e != ESP_OK) {
		ESP_LOGW(__func__, "esp_wifi_scan_get_ap_records() failed, reason = %s", esp_err_to_name(e));
		return e;
	}
	printf("Total APs scanned = %u, actual AP number ap_info holds = %u\n", ap_count, number);
	printf(FMT_AP_HEADER "\n", "SSID", "RSSI", "Chan", "Authmode", "Pairwise", "Group");
	for (int i = 0; i < number; i++) {
		printf(FMT_AP_ROW "\n",
		ap_info[i].ssid,
		ap_info[i].rssi,
		ap_info[i].primary,
		wifi_auth_mode_str(ap_info[i].authmode),
		wifi_cipher_type_str(ap_info[i].pairwise_cipher),
		wifi_cipher_type_str(ap_info[i].group_cipher));
	}
	return ESP_OK;
}
