#include "hardware_wifi.h"

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_event.h>
#include <freertos/task.h>

#define EXAMPLE_NETIF_DESC_STA "example_netif_sta"

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
static esp_netif_t *sta_netif = NULL;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT        BIT0
#define WIFI_FAIL_BIT             BIT1
#define EXAMPLE_ESP_MAXIMUM_RETRY 10

static int s_retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGI(__func__, "retry to connect to the AP");
		} else {
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		}
		ESP_LOGI(__func__, "connect to the AP fail");
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
		ESP_LOGI(__func__, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

esp_err_t Hardware_wifi_start()
{
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
	// Warning: the interface desc is used in tests to capture actual connection details (IP, gw, mask)
	esp_netif_config.if_desc = EXAMPLE_NETIF_DESC_STA;
	esp_netif_config.route_prio = 128;
	sta_netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
	esp_wifi_set_default_wifi_sta_handlers();
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());
	return ESP_OK;
}

esp_err_t Hardware_wifi_join(const char *ssid, const char *pw, int timeout_ms)
{
	esp_err_t e;

	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));
	ESP_LOGI(__func__, "Join SSID=%s, pw=%s, timeout_ms=%i", ssid, pw, timeout_ms);
	wifi_config_t wifi_config = {0};
	strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
	if (pw) {
		strlcpy((char *)wifi_config.sta.password, pw, sizeof(wifi_config.sta.password));
	}
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_LOGI(__func__, "esp_wifi_start()");
	ESP_ERROR_CHECK(esp_wifi_start());
	esp_wifi_connect();

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	 * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
	WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
	pdFALSE,
	pdFALSE,
	portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(__func__, "connected to ap SSID:%s password:%s", ssid, pw);
		return ESP_OK;
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGE(__func__, "Failed to connect, SSID:%s password:%s", ssid, pw);
		return ESP_FAIL;
	} else {
		ESP_LOGE(__func__, "UNEXPECTED EVENT");
		return ESP_FAIL;
	}
}

void Hardware_print_ip()
{
	esp_netif_ip_info_t ip_info;
	esp_netif_get_ip_info(sta_netif, &ip_info);
	printf("My IP: " IPSTR "\n", IP2STR(&ip_info.ip));
	printf("My GW: " IPSTR "\n", IP2STR(&ip_info.gw));
	printf("My NETMASK: " IPSTR "\n", IP2STR(&ip_info.netmask));
}