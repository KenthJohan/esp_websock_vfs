#include "systems/system_term.h"
#include "myware/myware_nvs.h"
#include "hardware/hardware_wifi.h"

#include <esp_netif.h>
#include <esp_eth.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>

void app_main(void)
{
	Myware_nvs_init();
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	system_term_t system_term;
	system_term_init(&system_term);

	Hardware_wifi_start();
}