#pragma once
#include <esp_err.h>
#include <esp_http_server.h>

esp_err_t http_wscool_init(httpd_handle_t server);