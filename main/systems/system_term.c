#include "system_term.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <esp_log.h>
#include <esp_console.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/portmacro.h>

#include <driver/uart.h>
#include <driver/uart_vfs.h>
#include <driver/usb_serial_jtag.h>

#include <esp_log.h>
#include <linenoise/linenoise.h>

#include "console/console_nvs.h"
#include "console/console_wifi.h"
#include "console/console_os.h"

#define CONSOLE_MAX_CMDLINE_ARGS   8
#define CONSOLE_MAX_CMDLINE_LENGTH 256

static esp_err_t private_uart_init(system_term_t *system)
{
	// Disable loggin when reconfiguring uart0:
	fflush(stdout);
	fsync(fileno(stdout));

	uart_vfs_dev_port_set_rx_line_endings(UART_NUM_0, ESP_LINE_ENDINGS_CR);
	/* Move the caret to the beginning of the next line on '\n' */
	uart_vfs_dev_port_set_tx_line_endings(UART_NUM_0, ESP_LINE_ENDINGS_CRLF);

	uart_config_t uart_config = {
	.baud_rate = 230400,
	.data_bits = UART_DATA_8_BITS,
	.parity = UART_PARITY_DISABLE,
	.stop_bits = UART_STOP_BITS_1,
	.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
	.source_clk = UART_SCLK_DEFAULT,
	};
	esp_err_t e;

	e = uart_driver_install(UART_NUM_0, 1024, 1024, 20, NULL, 0);
	if (e != ESP_OK) {
		return e;
	}

	e = uart_param_config(UART_NUM_0, &uart_config);
	if (e != ESP_OK) {
		return e;
	}

	e = uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	if (e != ESP_OK) {
		return e;
	}

	uart_vfs_dev_use_driver(UART_NUM_0);

	setvbuf(stdin, NULL, _IONBF, 0);

	// Loggin enabled from here
	return e;
}

static void private_console_init(system_term_t *system)
{
	/* Initialize the console */
	esp_console_config_t console_config = {
	.max_cmdline_args = CONSOLE_MAX_CMDLINE_ARGS,
	.max_cmdline_length = CONSOLE_MAX_CMDLINE_LENGTH,
#if CONFIG_LOG_COLORS
	.hint_color = atoi(LOG_COLOR_CYAN)
#endif
	};
	ESP_ERROR_CHECK(esp_console_init(&console_config));

	/* Configure linenoise line completion library */
	/* Enable multiline editing. If not set, long commands will scroll within
	 * single line.
	 */
	linenoiseSetMultiLine(1);

	/* Tell linenoise where to get command completions and hints */
	linenoiseSetCompletionCallback(&esp_console_get_completion);
	linenoiseSetHintsCallback((linenoiseHintsCallback *)&esp_console_get_hint);

	/* Set command history size */
	linenoiseHistorySetMaxLen(100);

	/* Set command maximum length */
	linenoiseSetMaxLineLen(console_config.max_cmdline_length);

	/* Don't return empty lines */
	linenoiseAllowEmpty(false);

#if CONFIG_CONSOLE_STORE_HISTORY
	/* Load command history from filesystem */
	linenoiseHistoryLoad(history_path);
#endif // CONFIG_CONSOLE_STORE_HISTORY

	/* Figure out if the terminal supports escape sequences */
	const int probe_status = linenoiseProbe();
	if (probe_status) { /* zero indicates success */
		linenoiseSetDumbMode(1);
	}

	esp_console_register_help_command();
}

static void private_task_term(system_term_t *system)
{
	assert(system != NULL);
	ESP_LOGI(__func__, "begin reading uart %i", UART_NUM_0);
	for (;;) {
		char *line = linenoise(LOG_COLOR_I CONFIG_IDF_TARGET ">" LOG_RESET_COLOR);
		// vTaskDelay(pdMS_TO_TICKS(5000));
		if (line == NULL) {
			if (errno == EAGAIN) {
				ESP_LOGI(__func__, "CTRL-C");
			}
			continue;
		}

		/* Add the command to the history if not empty*/
		if (strlen(line) > 0) {
			linenoiseHistoryAdd(line);
			/* Save command history to filesystem */
			// linenoiseHistorySave(HISTORY_PATH);
		}

		/* Try to run the command */
		int ret;
		esp_err_t err = esp_console_run(line, &ret);
		if (err == ESP_ERR_NOT_FOUND) {
			printf("Unrecognized command\n");
		} else if (err == ESP_ERR_INVALID_ARG) {
			// command was empty
		} else if (err == ESP_OK && ret != ESP_OK) {
			printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(ret));
		} else if (err != ESP_OK) {
			printf("Internal error: %s\n", esp_err_to_name(err));
		}
		/* linenoise allocates line buffer on the heap, so need to free it */
		linenoiseFree(line);
	}
	for (;;) {
		vTaskDelay(pdMS_TO_TICKS(1000));
		printf("loop\n");
	}
	vTaskDelete(NULL);
}

void system_term_init(system_term_t *system)
{
	private_uart_init(system);
	private_console_init(system);
	console_nvs_init();
	console_wifi_init();
	console_os_init();

	if (linenoiseIsDumbMode()) {
		printf("\n"
		       "Your terminal application does not support escape sequences.\n"
		       "Line editing and history features are disabled.\n"
		       "On Windows, try using Putty instead.\n");
	}

	xTaskCreate((TaskFunction_t)private_task_term, "my_term", 1024 * 10, system, 10, NULL);
	return;
}
