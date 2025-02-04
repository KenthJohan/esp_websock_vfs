#include "systems/system_term.h"
#include "systems/system_web.h"
#include "myware/myware_nvs.h"
#include "hardware/hardware_wifi.h"
#include "web/web_server.h"

#include <esp_netif.h>
#include <esp_eth.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <driver/uart.h>

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

system_term_t system_term = {0};
system_web_t system_web = {0};

int my_vprintf(const char *fmt, va_list args)
{
	char buf[1024] = {0};
	int n = vsnprintf(buf, sizeof(buf), fmt, args);
	if (n < 0) {
		return n;
	}
	uart_write_bytes(UART_NUM_0, buf, n);
	char *item;
	UBaseType_t res;
	res = xRingbufferSendAcquire(system_web.rb_tx, (void **)&item, n, 0);
	if (res != pdTRUE) {
		return 0;
	}
	res = xRingbufferSendComplete(system_web.rb_tx, &item);
	if (res != pdTRUE) {
		printf("Failed to send item\n");
	}
	return n;
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
	system_web_init(&system_web);
	esp_log_set_vprintf(my_vprintf);
}

void app_main(void)
{
	Myware_nvs_init();
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

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