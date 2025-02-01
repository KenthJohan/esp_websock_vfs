#include "console_os.h"

#include <esp_console.h>
#include <esp_log.h>

static int cb_tasks(int argc, char **argv)
{
	const size_t bytes_per_task = 40; /* see vTaskList description */
	char *task_list_buffer = malloc(uxTaskGetNumberOfTasks() * bytes_per_task);
	if (task_list_buffer == NULL) {
		ESP_LOGE(__func__, "failed to allocate buffer for vTaskList output");
		return 1;
	}
	fputs("Task Name\tStatus\tPrio\tHWM\tTask#", stdout);
#ifdef CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
	fputs("\tAffinity", stdout);
#endif
	fputs("\n", stdout);
	vTaskList(task_list_buffer);
	fputs(task_list_buffer, stdout);
	free(task_list_buffer);
	return 0;
}

static int cb_restart(int argc, char **argv)
{
	ESP_LOGI(__func__, "Restarting");
	esp_restart();
}

static int cb_heap(int argc, char **argv)
{
	uint32_t heap_size = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
	printf("available heap: %" PRIu32 "B\n", esp_get_free_heap_size());
	printf("min heap size: %" PRIu32 "B\n", heap_size);
	return 0;
}

void console_os_init()
{

	const esp_console_cmd_t cmd_tasks = {
	.command = "tasks",
	.help = "Get information about running tasks",
	.hint = NULL,
	.func = &cb_tasks,
	};

	const esp_console_cmd_t cmd_restart = {
	.command = "restart",
	.help = "Software reset of the chip",
	.hint = NULL,
	.func = &cb_restart,
	};

	const esp_console_cmd_t cmd_heap = {
	.command = "heap",
	.help = "Show heap sizes",
	.hint = NULL,
	.func = &cb_heap,
	};

	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_heap));
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_tasks));
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_restart));

	return;
}