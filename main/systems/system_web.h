#pragma once
#include <freertos/ringbuf.h>
#include <esp_err.h>

typedef struct {
	RingbufHandle_t rb_rx;
	RingbufHandle_t rb_tx;
	void *server;
} system_web_t;

esp_err_t system_web_init(system_web_t *system);
