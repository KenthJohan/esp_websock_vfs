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

static esp_err_t echo_handler(httpd_req_t *req)
{
	if (req->method == HTTP_GET) {
		ESP_LOGI(__func__, "Handshake done, the new connection was opened");
		return ESP_OK;
	}

	print_all_ws_fds(req);

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
		if (ws_pkt.len > 0) {
			xQueueSend(ws_rx_queue, ws_pkt.payload, portMAX_DELAY);
		}
		ESP_LOGI(__func__, "Got packet with message: %s", ws_pkt.payload);
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

static void uart_log_str(const char *data)
{
	uart_write_bytes(UART_NUM_0, data, strlen(data));
}

static ssize_t vfs_write(int fd, const void *data, size_t size)
{
	char buf[128] = {0};
	sniprintf(buf, sizeof(buf), "vfs_write: fd: %d, data: %s\n", fd, (const char *)data);
	uart_log_str(buf);
	if (fd < 0) {
		errno = EBADF;
		return -1;
	}
	return 0;
}

static ssize_t vfs_read(int fd, void *data, size_t size)
{
	char buf[128] = {0};
	sniprintf(buf, sizeof(buf), "vfs_read: fd: %d, size: %d, data: %s\n", fd, size, (char *)data);
	uart_log_str(buf);
	// Read data from the WebSocket RX queue
	if (xQueueReceive(ws_rx_queue, data, portMAX_DELAY) == pdTRUE) {
		return strlen((char *)data); // Return the number of bytes read
	}
	return 0;
}

static int vfs_open(const char *path, int flags, int mode)
{
	char buf[128] = {0};
	sniprintf(buf, sizeof(buf), "vfs_open: path: %s, flags: %d, mode: %d\n", path, flags, mode);
	uart_log_str(buf);
	return 0;
}

static int vfs_fstat(int fd, struct stat *st)
{
	st->st_mode = S_IFCHR;
	char buf[128] = {0};
	sniprintf(buf, sizeof(buf), "vfs_fstat: fd: %d, st_mode: %ld\n", fd, st->st_mode);
	uart_log_str(buf);
	return 0;
}

esp_err_t http_wscool_init(httpd_handle_t server)
{

	httpd_uri_t uri_ws = {
	.uri = "/ws",
	.method = HTTP_GET,
	.handler = echo_handler,
	.user_ctx = NULL,
	.is_websocket = true};

	httpd_register_uri_handler(server, &uri_ws);

	esp_vfs_t vfs = {};
	vfs.flags = ESP_VFS_FLAG_DEFAULT;
	vfs.read = vfs_read;
	vfs.write = vfs_write;
	vfs.open = vfs_open;
	vfs.fstat = vfs_fstat,

	// Create a queue for WebSocket RX data
	ws_rx_queue = xQueueCreate(10, 128); // Adjust queue size as needed

	esp_vfs_register("/dev/wscool", &vfs, NULL);

	FILE *f = fopen("/dev/wscool", "w+");
	if (f == NULL) {
		ESP_LOGE(__func__, "Failed to open VFS file");
		return ESP_FAIL;
	}

	if (f) {
		fflush(stdout);
		fflush(stderr);
		fflush(stdin);
		stdout = f;
		stderr = f;
		setvbuf(stdout, NULL, _IONBF, 0);
		setvbuf(stderr, NULL, _IONBF, 0);
		setvbuf(stdin, NULL, _IONBF, 0);
	}

	// stdout = f;
	// stdin = f;

	return ESP_OK;
}