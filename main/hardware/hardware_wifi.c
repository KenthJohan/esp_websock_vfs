#include "hardware_wifi.h"

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_event.h>
#include <freertos/task.h>

#include "wifi_tostr.h"

#define EXAMPLE_NETIF_DESC_STA             "example_netif_sta"
#define CONFIG_EXAMPLE_WIFI_CONN_MAX_RETRY 10

// FreeRTOS event group to signal when we are connected
// NOTE: We can add more bits to wait for here when needed
#define WIFI_CONNECTED_BIT        BIT0
#define EXAMPLE_ESP_MAXIMUM_RETRY 10
static EventGroupHandle_t s_wifi_event_group = NULL;
static esp_netif_t *sta_netif = NULL;
esp_event_handler_instance_t instance_any_id = NULL;
esp_event_handler_instance_t instance_got_ip = NULL;

static void event_log(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT) {
		ESP_LOGI(__func__, "WIFI_EVENT: %s", wifi_event_tostr(event_id));
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
		ESP_LOGI(__func__, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
	} else {
		ESP_LOGI(__func__, "Event base: %s, Event ID: %d", event_base, event_id);
	}
}

static void event_ip_sta_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
}

esp_err_t Hardware_wifi_start()
{
	esp_err_t e;

	if (sta_netif != NULL) {
		ESP_LOGE(__func__, "sta_netif is not null");
		return ESP_FAIL;
	}

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	s_wifi_event_group = xEventGroupCreate();
	if (s_wifi_event_group == NULL) {
		ESP_LOGE(__func__, "xEventGroupCreate() failed");
		return ESP_FAIL;
	}

	e = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_log, NULL, &instance_any_id);
	if (e != ESP_OK) {
		ESP_LOGE(__func__, "esp_event_handler_instance_register() failed, reason = %s", esp_err_to_name(e));
		return e;
	}
	// This is used when connecting to an AP later on:
	e = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_ip_sta_got_ip, NULL, &instance_got_ip);
	if (e != ESP_OK) {
		ESP_LOGE(__func__, "esp_event_handler_instance_register() failed, reason = %s", esp_err_to_name(e));
		return e;
	}

	ESP_LOGI(__func__, "esp_wifi_init()");
	e = esp_wifi_init(&cfg);
	if (e != ESP_OK) {
		ESP_LOGE(__func__, "esp_wifi_init() failed, reason = %s", esp_err_to_name(e));
		return e;
	}

	// Warning: the interface desc is used in tests to capture actual connection details (IP, gw, mask)
	esp_netif_inherent_config_t config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
	config.if_desc = EXAMPLE_NETIF_DESC_STA;
	config.route_prio = 128;
	ESP_LOGI(__func__, "esp_netif_create_wifi()");
	sta_netif = esp_netif_create_wifi(WIFI_IF_STA, &config);
	if (sta_netif == NULL) {
		ESP_LOGE(__func__, "esp_netif_create_wifi() failed");
		return ESP_FAIL;
	}

	ESP_LOGI(__func__, "esp_wifi_set_default_wifi_sta_handlers()");
	e = esp_wifi_set_default_wifi_sta_handlers();
	if (e != ESP_OK) {
		ESP_LOGE(__func__, "esp_wifi_set_default_wifi_sta_handlers() failed, reason = %s", esp_err_to_name(e));
		return e;
	}

	e = esp_wifi_set_storage(WIFI_STORAGE_RAM);
	if (e != ESP_OK) {
		ESP_LOGE(__func__, "esp_wifi_set_storage() failed, reason = %s", esp_err_to_name(e));
		return e;
	}
	e = esp_wifi_set_mode(WIFI_MODE_STA);
	if (e != ESP_OK) {
		ESP_LOGE(__func__, "esp_wifi_set_mode() failed, reason = %s", esp_err_to_name(e));
		return e;
	}

	ESP_LOGI(__func__, "esp_wifi_start()");
	e = esp_wifi_start();
	if (e != ESP_OK) {
		ESP_LOGE(__func__, "esp_wifi_start() failed, reason = %s", esp_err_to_name(e));
		return e;
	}

	ESP_LOGI(__func__, "SUCCESS");
	return e;
}

esp_err_t Hardware_wifi_stop()
{
	esp_err_t e;
	e = esp_wifi_stop();
	if (e != ESP_OK) {
		ESP_LOGE(__func__, "esp_wifi_stop() failed, reason = %s", esp_err_to_name(e));
		return e;
	}
	e = esp_wifi_deinit();
	if (e != ESP_OK) {
		ESP_LOGE(__func__, "esp_wifi_deinit() failed, reason = %s", esp_err_to_name(e));
		return e;
	}

	if (s_wifi_event_group) {
		vEventGroupDelete(s_wifi_event_group);
		s_wifi_event_group = NULL;
	}

	if (sta_netif) {
		e = esp_wifi_clear_default_wifi_driver_and_handlers(sta_netif);
		if (e != ESP_OK) {
			ESP_LOGE(__func__, "esp_wifi_clear_default_wifi_driver_and_handlers() failed, reason = %s", esp_err_to_name(e));
			return e;
		}
		esp_netif_destroy(sta_netif);
		sta_netif = NULL;
	}

	if (instance_any_id) {
		e = esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
		if (e != ESP_OK) {
			ESP_LOGE(__func__, "esp_event_handler_instance_unregister() failed, reason = %s", esp_err_to_name(e));
			return e;
		}
		instance_any_id = NULL;
	}

	if (instance_got_ip) {
		e = esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip);
		if (e != ESP_OK) {
			ESP_LOGE(__func__, "esp_event_handler_instance_unregister() failed, reason = %s", esp_err_to_name(e));
			return e;
		}
		instance_got_ip = NULL;
	}
	return e;
}

esp_err_t Hardware_wifi_connect(const char *ssid, const char *pw, int timeout_ms)
{
	esp_err_t e;
	if (sta_netif == NULL) {
		ESP_LOGE(__func__, "sta_netif is NULL");
		return ESP_FAIL;
	}

	wifi_config_t wifi_config = {0};
	strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
	if (pw) {
		strlcpy((char *)wifi_config.sta.password, pw, sizeof(wifi_config.sta.password));
	}

	ESP_LOGI(__func__, "esp_wifi_set_config() SSID=%s, pw=%s, timeout_ms=%i", ssid, pw, timeout_ms);
	e = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
	if (e != ESP_OK) {
		ESP_LOGE(__func__, "esp_wifi_set_config() failed, reason = %s", esp_err_to_name(e));
		return e;
	}

	// Waiting until the connection is established (WIFI_CONNECTED_BIT)
	for (int i = 0; i < CONFIG_EXAMPLE_WIFI_CONN_MAX_RETRY; i++) {
		e = esp_wifi_connect();
		if (e != ESP_OK) {
			ESP_LOGE(__func__, "esp_wifi_connect() failed, reason = %s", esp_err_to_name(e));
			return e;
		}
		// NOTE: We can add more bits to wait for here when needed
		EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
		if (bits & WIFI_CONNECTED_BIT) {
			ESP_LOGI(__func__, "connected to ap SSID:%s password:%s", ssid, pw);
			break;
		} else {
			ESP_LOGE(__func__, "UNEXPECTED EVENT");
			return ESP_FAIL;
		}
	}
	return e;
}

esp_err_t Hardware_wifi_disconnect()
{
	esp_err_t e;
	if (sta_netif == NULL) {
		ESP_LOGE(__func__, "sta_netif is NULL");
		return ESP_FAIL;
	}
	e = esp_wifi_disconnect();
	if (e != ESP_OK) {
		ESP_LOGE(__func__, "esp_wifi_disconnect() failed, reason = %s", esp_err_to_name(e));
		return e;
	}
	return e;
}

esp_err_t Hardware_wifi_print_ip(FILE *f)
{
	if (sta_netif == NULL) {
		ESP_LOGE(__func__, "sta_netif is NULL");
		return ESP_FAIL;
	}
	esp_netif_ip_info_t info = {0};
	esp_netif_get_ip_info(sta_netif, &info);
	fprintf(f, "IP: " IPSTR "\n", IP2STR(&info.ip));
	fprintf(f, "GW: " IPSTR "\n", IP2STR(&info.gw));
	fprintf(f, "NETMASK: " IPSTR "\n", IP2STR(&info.netmask));
	return ESP_OK;
}

#define DEFAULT_SCAN_LIST_SIZE 20

#define FMT_AP_HEADER "%-40s %5s %5s %-20s %-10s %-10s"
#define FMT_AP_ROW    "%-40s %5i %5i %-20s %-10s %-10s"

esp_err_t Hardware_wifi_scanap(void)
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
