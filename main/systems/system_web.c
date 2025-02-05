#include "system_web.h"

#include <esp_log.h>
#include <esp_http_server.h>

static esp_err_t echo_handler(httpd_req_t *req)
{
	if (req->method == HTTP_GET) {
		ESP_LOGI(__func__, "Handshake done, the new connection was opened");
		return ESP_OK;
	}

	httpd_ws_frame_t ws_pkt;
	uint8_t *buf = NULL;
	memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
	ws_pkt.type = HTTPD_WS_TYPE_TEXT;
	/* Set max_len = 0 to get the frame len */
	esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
	if (ret != ESP_OK) {
		ESP_LOGE(__func__, "httpd_ws_recv_frame failed to get frame len with %d", ret);
		return ret;
	}
	ESP_LOGI(__func__, "frame len is %d", ws_pkt.len);
	if (ws_pkt.len) {
		/* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
		buf = calloc(1, ws_pkt.len + 1);
		if (buf == NULL) {
			ESP_LOGE(__func__, "Failed to calloc memory for buf");
			return ESP_ERR_NO_MEM;
		}
		ws_pkt.payload = buf;
		/* Set max_len = ws_pkt.len to get the frame payload */
		ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
		if (ret != ESP_OK) {
			ESP_LOGE(__func__, "httpd_ws_recv_frame failed with %d", ret);
			free(buf);
			return ret;
		}
		ESP_LOGI(__func__, "Got packet with message: %s,,, %i", ws_pkt.payload, strcmp((char *)ws_pkt.payload, "Trigger async"));
		free(buf);
	}
	return ret;
}

static esp_err_t print_all_ws_fds(httpd_handle_t server, uint8_t *buf, size_t buf_len)
{
	size_t n = CONFIG_LWIP_MAX_LISTENING_TCP;
	int fds[CONFIG_LWIP_MAX_LISTENING_TCP] = {0};
	esp_err_t ret = httpd_get_client_list(server, &n, fds);
	if (ret != ESP_OK) {
		return ret;
	}
	for (int i = 0; i < n; i++) {
		httpd_ws_client_info_t info = httpd_ws_get_fd_info(server, fds[i]);
		if (info == HTTPD_WS_CLIENT_WEBSOCKET) {
			printf("ws-fd: %d\n", fds[i]);
			httpd_ws_frame_t pkt;
			memset(&pkt, 0, sizeof(httpd_ws_frame_t));
			pkt.payload = (uint8_t *)buf;
			pkt.len = buf_len;
			pkt.type = HTTPD_WS_TYPE_TEXT;
			httpd_ws_send_frame_async(server, fds[i], &pkt);
		}
	}
	return ESP_OK;
}

static void private_task_my_wstx(system_web_t *system)
{
	assert(system != NULL);
	ESP_LOGI(__func__, "init");
	while (1) {
		if (system->rb_tx == NULL) {
			vTaskDelay(pdMS_TO_TICKS(1000));
			continue;
		}
		size_t item_size;
		uint8_t *item = xRingbufferReceive(system->rb_tx, &item_size, pdMS_TO_TICKS(1000));
		if (item == NULL) {
			continue;
		}
		print_all_ws_fds(system->server, item, item_size);
		vRingbufferReturnItem(system->rb_tx, (void *)item);
	}
	vTaskDelete(NULL);
}

esp_err_t system_web_init(system_web_t *system)
{
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.max_uri_handlers = 20;
	config.uri_match_fn = httpd_uri_match_wildcard;

	ESP_LOGI(__func__, "Starting HTTP Server on port: '%d'", config.server_port);
	if (httpd_start(&server, &config) != ESP_OK) {
		ESP_LOGE(__func__, "Failed to start file server!");
		return ESP_FAIL;
	}
	system->server = server;

	system->rb_tx = xRingbufferCreate(1028, RINGBUF_TYPE_NOSPLIT);
	if (system->rb_tx == NULL) {
		printf("Failed to create ring buffer\n");
	}

	httpd_uri_t uri_ws = {
	.uri = "/ws",
	.method = HTTP_GET,
	.handler = echo_handler,
	.user_ctx = NULL,
	.is_websocket = true};
	httpd_register_uri_handler(server, &uri_ws);

	xTaskCreate((TaskFunction_t)private_task_my_wstx, "my_web", 1024 * 10, system, 10, NULL);
	return ESP_OK;
}
