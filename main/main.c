#include "systems/system_term.h"
#include "myware/myware_nvs.h"
#include "hardware/hardware_wifi.h"
#include "web/web_server.h"

#include <esp_netif.h>
#include <esp_eth.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>

static void setup_wifi_start()
{
	esp_err_t e;
	bool wifi_start;
	e = Myware_nvs_get_bool_verbose("wifi_start", &wifi_start);
	if (e != ESP_OK) {
		ESP_LOGE(__func__, "Myware_nvs_get_bool_verbose(wifi_start) failed with %d", e);
		return;
	}
	if (wifi_start == false) {
		return;
	}
	Hardware_wifi_start();
}

static void setup_wifi_connect()
{
	esp_err_t e;
	bool wifi_connect;
	e = Myware_nvs_get_bool_verbose("wifi_connect", &wifi_connect);
	if (e != ESP_OK) {
		ESP_LOGE(__func__, "Myware_nvs_get_bool_verbose(wifi_connect) failed with %d", e);
		return;
	}
	if (wifi_connect == false) {
		return;
	}
	char wifi_ssid[32];
	char wifi_pw[64];
	e = Myware_nvs_get_str_verbose("wifi_ssid", wifi_ssid, sizeof(wifi_ssid));
	if (e != ESP_OK) {
		ESP_LOGE(__func__, "Myware_nvs_get_str_verbose(wifi_ssid) failed with %d", e);
		return;
	}
	e = Myware_nvs_get_str_verbose("wifi_pw", wifi_pw, sizeof(wifi_pw));
	if (e != ESP_OK) {
		ESP_LOGE(__func__, "Myware_nvs_get_str_verbose(wifi_ssid) failed with %d", e);
		return;
	}
	Hardware_wifi_connect(wifi_ssid, wifi_pw, 0);
}

static void setup_webserver_start()
{
	esp_err_t e;
	bool web_start;
	e = Myware_nvs_get_bool_verbose("web_start", &web_start);
	if (e != ESP_OK) {
		ESP_LOGE(__func__, "Myware_nvs_get_bool_verbose(web_start) failed with %d", e);
		return;
	}
	if (web_start == false) {
		return;
	}
	web_server_init("");
}

void app_main(void)
{
	Myware_nvs_init();
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	system_term_t system_term;
	system_term_init(&system_term);

	Myware_nvs_set_bool_verbose("web_start", false, true);
	Myware_nvs_set_bool_verbose("wifi_start", false, true);
	Myware_nvs_set_bool_verbose("wifi_connect", false, true);
	Myware_nvs_set_str_verbose("wifi_ssid", "<router_ssid>", true);
	Myware_nvs_set_str_verbose("wifi_pw", "<router_pw>", true);

	setup_wifi_start();
	setup_wifi_connect();
	setup_webserver_start();
}