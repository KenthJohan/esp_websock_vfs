#include "console_web.h"

#include <esp_console.h>
#include <esp_log.h>
#include "web/web_server.h"

static int cb_start(int argc, char **argv)
{
	ESP_LOGI(__func__, "Starting web server... NOT IMPLEMENTED");
	return 0;
}

void console_web_init(void)
{
	const esp_console_cmd_t cmd_start = {
	.command = "web-start",
	.help = "Start the web server",
	.hint = NULL,
	.func = &cb_start,
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_start));
}