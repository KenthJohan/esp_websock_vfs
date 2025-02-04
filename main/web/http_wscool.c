#include "http_wscool.h"
#include <esp_err.h>
#include <esp_log.h>
#include <esp_http_server.h>

#include <esp_vfs.h>
#include <sys/errno.h>
#include <sys/reent.h>
#include <driver/uart.h>

struct async_resp_arg {
	httpd_handle_t hd;
	int fd;
};

QueueHandle_t ws_rx_queue;

static void ws_async_send(void *arg)
{
	static const char *data = "Async data";
	struct async_resp_arg *resp_arg = arg;
	httpd_handle_t hd = resp_arg->hd;
	int fd = resp_arg->fd;
	httpd_ws_frame_t ws_pkt;
	memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
	ws_pkt.payload = (uint8_t *)data;
	ws_pkt.len = strlen(data);
	ws_pkt.type = HTTPD_WS_TYPE_TEXT;

	httpd_ws_send_frame_async(hd, fd, &ws_pkt);
	free(resp_arg);
}

static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)
{
	struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
	if (resp_arg == NULL) {
		return ESP_ERR_NO_MEM;
	}
	resp_arg->hd = req->handle;
	resp_arg->fd = httpd_req_to_sockfd(req);
	esp_err_t ret = httpd_queue_work(handle, ws_async_send, resp_arg);
	if (ret != ESP_OK) {
		free(resp_arg);
	}
	return ret;
}

static esp_err_t print_all_ws_fds(httpd_req_t *req)
{
	size_t n = CONFIG_LWIP_MAX_LISTENING_TCP;
	int fds[CONFIG_LWIP_MAX_LISTENING_TCP] = {0};

	esp_err_t ret = httpd_get_client_list(req->handle, &n, fds);

	if (ret != ESP_OK) {
		return ret;
	}

	for (int i = 0; i < n; i++) {
		httpd_ws_client_info_t info = httpd_ws_get_fd_info(req->handle, fds[i]);
		if (info == HTTPD_WS_CLIENT_WEBSOCKET) {
			printf("ws-fd: %d\n", fds[i]);
			// httpd_ws_send_frame_async(server_handle, fds[i], ws_pkt);
		}
	}

	return ESP_OK;
}

static int ws_fd = -1;

static esp_err_t echo_handler(httpd_req_t *req)
{
	if (req->method == HTTP_GET) {
		ESP_LOGI(__func__, "Handshake done, the new connection was opened");
		return ESP_OK;
	}

	print_all_ws_fds(req);
	ws_fd = httpd_req_to_sockfd(req);

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
		// Write received data to the queue for stdin
		/*
		if (ws_pkt.len > 0) {
		    xQueueSend(ws_rx_queue, ws_pkt.payload, portMAX_DELAY);
		}
		*/
		ESP_LOGI(__func__, "Got packet with message: %s,,, %i", ws_pkt.payload, strcmp((char *)ws_pkt.payload, "Trigger async"));
	}
	ESP_LOGI(__func__, "Packet type: %d", ws_pkt.type);
	if (ws_pkt.type == HTTPD_WS_TYPE_TEXT &&
	    strcmp((char *)ws_pkt.payload, "Trigger async") == 0) {
		free(buf);
		return trigger_async_send(req->handle, req);
	}

	ret = httpd_ws_send_frame(req, &ws_pkt);
	if (ret != ESP_OK) {
		ESP_LOGE(__func__, "httpd_ws_send_frame failed with %d", ret);
	}
	free(buf);
	return ret;
}

#define STATIC_BUF_SIZE 128
static httpd_handle_t static_server_handle = NULL;
static char static_buf[STATIC_BUF_SIZE] = {0};

int my_vprintf(const char *fmt, va_list args)
{
	// char buf[128] = {0};
	char *buf = static_buf;
	int n = vsnprintf(buf, STATIC_BUF_SIZE, fmt, args);
	if (n < 0) {
		return n;
	}
	uart_write_bytes(UART_NUM_0, buf, n);

	if (ws_fd >= 0) {
		httpd_ws_frame_t ws_pkt;
		memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
		ws_pkt.payload = (uint8_t *)buf;
		ws_pkt.len = n;
		ws_pkt.type = HTTPD_WS_TYPE_TEXT;
		httpd_ws_send_frame_async(static_server_handle, ws_fd, &ws_pkt);
	}
	return n;
}

static void private_task_my_wstx(void *system)
{
	assert(system != NULL);
	ESP_LOGI(__func__, "init");

	vTaskDelete(NULL);
}

esp_err_t http_wscool_init(httpd_handle_t server)
{
	static_server_handle = server;

	httpd_uri_t uri_ws = {
	.uri = "/ws",
	.method = HTTP_GET,
	.handler = echo_handler,
	.user_ctx = NULL,
	.is_websocket = true};

	httpd_register_uri_handler(server, &uri_ws);

	// Create a queue for WebSocket RX data
	ws_rx_queue = xQueueCreate(10, 128); // Adjust queue size as needed

	esp_log_set_vprintf(my_vprintf);

	xTaskCreate((TaskFunction_t)private_task_my_wstx, "my_wstx", 1024 * 10, system, 10, NULL);

	return ESP_OK;
}