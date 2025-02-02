#include "console_wifi.h"

#include <esp_console.h>
#include <argtable3/argtable3.h>
#include <linenoise/linenoise.h>
#include <esp_log.h>

#include "hardware/hardware_wifi.h"
#include "myware/myware_nvs.h"

static struct {
	struct {
		struct arg_str *ssid;
		struct arg_str *pw;
		struct arg_end *end;
	} wifi_cred;
} sargs;

static int cb_wifi_cred(void *context, int argc, char **argv)
{
	int nerrors = arg_parse(argc, argv, (void **)&sargs.wifi_cred);
	if (nerrors != 0) {
		arg_print_errors(stderr, sargs.wifi_cred.end, argv[0]);
		return 1;
	}
	char const *ssid = sargs.wifi_cred.ssid->sval[0];
	char const *pw = sargs.wifi_cred.pw->sval[0];
	ESP_LOGI(__func__, "Set WiFi cred to ssid:%s pw:%s", ssid, pw);
	Myware_nvs_set_str_verbose("wifi_ssid", ssid, false);
	Myware_nvs_set_str_verbose("wifi_pw", pw, false);
	return 0;
}

static int cb_wifi_scan(void *context, int argc, char **argv)
{
	esp_err_t e = Hardware_wifi_scanap();
	if (e != ESP_OK) {
		ESP_LOGW(__func__, "Hardware_wifi_scanap() failed, reason = %s", esp_err_to_name(e));
		return 1;
	}
	return 0;
}

static int cb_wifi_ip(void *context, int argc, char **argv)
{
	Hardware_wifi_print_ip(stdout);
	return 0;
}

static int cb_wifi_disconnect(void *context, int argc, char **argv)
{
	Hardware_wifi_disconnect();
	return 0;
}

static int cb_wifi_stop(void *context, int argc, char **argv)
{
	Hardware_wifi_stop();
	return 0;
}

static int cb_wifi_enable(void *context, int argc, char **argv)
{
	Myware_nvs_set_bool_verbose("wifi_enable", true, false);
	ESP_LOGW(__func__, "Restart needed to take effect");
	return 0;
}

static int cb_wifi_disable(void *context, int argc, char **argv)
{
	Myware_nvs_set_bool_verbose("wifi_enable", false, false);
	ESP_LOGW(__func__, "Restart needed to take effect");
	return 0;
}

static int cb_wifi_start(void *context, int argc, char **argv)
{
	Hardware_wifi_start();
	return 0;
}

static int cb_wifi_connect(void *context, int argc, char **argv)
{
	int nerrors = arg_parse(argc, argv, (void **)&sargs.wifi_cred);
	if (nerrors != 0) {
		arg_print_errors(stderr, sargs.wifi_cred.end, argv[0]);
		return 1;
	}
	char const *ssid = sargs.wifi_cred.ssid->sval[0];
	char const *pw = sargs.wifi_cred.pw->sval[0];
	ESP_LOGI(__func__, "Hardware_wifi_join(): ssid:%s pw:%s", ssid, pw);
	Hardware_wifi_connect(ssid, pw, 10000);
	return 0;
}

void console_wifi_init()
{
	sargs.wifi_cred.ssid = arg_str1(NULL, NULL, "<ssid>", "ssid");
	sargs.wifi_cred.pw = arg_str1(NULL, NULL, "<pw>", "pw");
	sargs.wifi_cred.end = arg_end(1);

	const esp_console_cmd_t cmd_wifi_cred = {
	.command = "wifi-cred",
	.help = "Set WiFi creds",
	.hint = NULL,
	.func_w_context = &cb_wifi_cred,
	.context = NULL,
	.argtable = &sargs.wifi_cred};

	const esp_console_cmd_t cmd_wifi_join = {
	.command = "wifi-connect",
	.help = "Connect wifi",
	.hint = NULL,
	.func_w_context = (esp_console_cmd_func_with_context_t)&cb_wifi_connect,
	.context = NULL,
	.argtable = &sargs.wifi_cred};

	const esp_console_cmd_t cmd_wifi_enable = {
	.command = "wifi-enable",
	.help = "Enable wifi",
	.hint = NULL,
	.func_w_context = &cb_wifi_enable,
	.context = NULL,
	};

	const esp_console_cmd_t cmd_wifi_disconnect = {
	.command = "wifi-disconnect",
	.help = "Disconnect wifi",
	.hint = NULL,
	.func_w_context = &cb_wifi_disconnect,
	.context = NULL,
	};

	const esp_console_cmd_t cmd_wifi_disable = {
	.command = "wifi-disable",
	.help = "Disable wifi",
	.hint = NULL,
	.func_w_context = &cb_wifi_disable,
	.context = NULL,
	};

	const esp_console_cmd_t cmd_wifi_stop = {
	.command = "wifi-stop",
	.help = "Stop wifi",
	.hint = NULL,
	.func_w_context = &cb_wifi_stop,
	.context = NULL,
	};

	const esp_console_cmd_t cmd_wifi_scan = {
	.command = "wifi-scan",
	.help = "Scan accesspoints",
	.hint = NULL,
	.func_w_context = &cb_wifi_scan,
	.context = NULL,
	};

	const esp_console_cmd_t cmd_wifi_ip = {
	.command = "wifi-ip",
	.help = "Show ip",
	.hint = NULL,
	.func_w_context = (esp_console_cmd_func_with_context_t)&cb_wifi_ip,
	.context = NULL,
	};

	const esp_console_cmd_t cmd_wifi_start = {
	.command = "wifi-start",
	.help = "Start wifi",
	.hint = NULL,
	.func_w_context = (esp_console_cmd_func_with_context_t)&cb_wifi_start,
	.context = NULL,
	};

	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_wifi_cred));
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_wifi_join));
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_wifi_enable));
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_wifi_disable));
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_wifi_disconnect));
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_wifi_scan));
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_wifi_ip));
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_wifi_stop));
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_wifi_start));

	return;
}