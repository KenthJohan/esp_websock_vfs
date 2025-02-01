#include <esp_err.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include "http_wscool.h"

esp_err_t web_server_init(const char *base_path)
{
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.max_uri_handlers = 20;

	/* Use the URI wildcard matching function in order to
	 * allow the same handler to respond to multiple different
	 * target URIs which match the wildcard scheme */
	config.uri_match_fn = httpd_uri_match_wildcard;

	ESP_LOGI(__func__, "Starting HTTP Server on port: '%d'", config.server_port);
	if (httpd_start(&server, &config) != ESP_OK) {
		ESP_LOGE(__func__, "Failed to start file server!");
		return ESP_FAIL;
	}

	http_wscool_init(server);

	return ESP_OK;
}
