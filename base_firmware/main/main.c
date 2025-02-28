#include "wasm_export.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include <inttypes.h>
#include "string.h"
#include "esp_heap_caps.h"

#define TAG "WASM"
#define WASM_PARTITION_NAME "wasm_bin"
#define WASM_APP_MEMORY_SIZE (1024 * 1024)
#define WASM_STACK_SIZE (64 * 1024)

void run_wasm_app() {
    ESP_LOGI(TAG, "Searching for WASM partition");

    const esp_partition_t *wasm_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, WASM_PARTITION_NAME);

    if (!wasm_partition) {
        ESP_LOGE(TAG, "Failed to find WebAssembly partition");
        return;
    }

    ESP_LOGI(TAG, "Found WASM partition at offset %" PRIx32 ", size: %" PRIu32 " bytes",
             wasm_partition->address, wasm_partition->size);

    ESP_LOGI(TAG, "Available heap before malloc: %" PRIu32 " bytes", esp_get_free_heap_size());

    size_t wasm_file_size = 64 * 1024;
    uint8_t *wasm_data = (uint8_t *)calloc(wasm_file_size, 1);
    if (!wasm_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for WASM file!");
        return;
    }

    ESP_LOGI(TAG, "Available heap after malloc: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Successfully allocated %" PRIu32 " bytes for WASM file", (uint32_t)wasm_file_size);

    ESP_LOGI(TAG, "Reading full WASM file from flash");

    esp_err_t err = esp_partition_read(wasm_partition, 0, wasm_data, wasm_file_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WASM file from flash! Error: %s", esp_err_to_name(err));
        free(wasm_data);
        return;
    }

    ESP_LOGI(TAG, "First 16 bytes of WASM file");
    for (int i = 0; i < 16; i++) {
        printf("%02X ", wasm_data[i]);
    }
    printf("\n");

    ESP_LOGI(TAG, "Comparing first 4 bytes: %02X %02X %02X %02X",
        wasm_data[0], wasm_data[1], wasm_data[2], wasm_data[3]);

    if (!(wasm_data[0] == 0x00 && wasm_data[1] == 0x61 && wasm_data[2] == 0x73 && wasm_data[3] == 0x6D)) {
        ESP_LOGE(TAG, "Invalid WASM header! Expected 00 61 73 6D but got: %02X %02X %02X %02X",
            wasm_data[0], wasm_data[1], wasm_data[2], wasm_data[3]);
        free(wasm_data);
        return;
    }

    ESP_LOGI(TAG, "Initializing WASM runtime");
    wasm_runtime_init();

    ESP_LOGI(TAG, "Loading WASM module");
    wasm_module_t module = wasm_runtime_load(wasm_data, wasm_file_size, NULL, 0);
    if (!module) {
        ESP_LOGE(TAG, "Failed to load WASM module");
        free(wasm_data);
        return;
    }

    ESP_LOGI(TAG, "Instantiating WASM module");
    wasm_module_inst_t module_inst = wasm_runtime_instantiate(module, WASM_APP_MEMORY_SIZE, WASM_STACK_SIZE, NULL, 0);
    if (!module_inst) {
        ESP_LOGE(TAG, "Failed to instantiate WASM module");
        wasm_runtime_unload(module);
        free(wasm_data);
        return;
    }

    ESP_LOGI(TAG, "Creating WASM execution environment");
    wasm_exec_env_t exec_env = wasm_runtime_create_exec_env(module_inst, WASM_STACK_SIZE);
    if (!exec_env) {
        ESP_LOGE(TAG, "Failed to create WASM execution environment");
        wasm_runtime_deinstantiate(module_inst);
        wasm_runtime_unload(module);
        free(wasm_data);
        return;
    }

    ESP_LOGI(TAG, "Searching for 'main' function");
    wasm_function_inst_t func = wasm_runtime_lookup_function(module_inst, "main");
    if (!func) {
        ESP_LOGE(TAG, "WASM function 'main' not found");
    } else {
        uint32_t args[1] = {0};
        wasm_runtime_call_wasm(exec_env, func, 0, args);
        ESP_LOGI(TAG, "WASM execution finished");
    }

    wasm_runtime_destroy_exec_env(exec_env);
    wasm_runtime_deinstantiate(module_inst);
    wasm_runtime_unload(module);
    wasm_runtime_destroy();
    free(wasm_data);
}

void app_main(void) {
    ESP_LOGI("BASE_FW", "Booting Base Firmware");

    if (esp_task_wdt_status(NULL) != ESP_OK) {
        esp_task_wdt_config_t wdt_config = {
            .timeout_ms = 10000,
            .idle_core_mask = 1,
            .trigger_panic = false
        };
        esp_task_wdt_init(&wdt_config);
        esp_task_wdt_add(NULL);
    }

    wasm_runtime_init();
    run_wasm_app();
    wasm_runtime_destroy();
}
