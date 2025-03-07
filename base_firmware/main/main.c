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
#define WASM_STACK_SIZE (128 * 1024)  

void run_wasm_app() {
    ESP_LOGI(TAG, "searching for WASM partition");

    // Find WASM partition named wasm_bin in ESP32 flash
    const esp_partition_t *wasm_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, WASM_PARTITION_NAME);

    if (!wasm_partition) {
        ESP_LOGE(TAG, "Failed to find WASM partition");
        return;
    }

    // Print where the WASM partition is stored in flash
    ESP_LOGI(TAG, "Found WASM partition at offset %" PRIx32 ", size: %" PRIu32 " bytes",
             wasm_partition->address, wasm_partition->size);
    
    // Allocates memory for WASM file (ensure this part is clear)
    size_t max_wasm_size = 128 * 1024;  
    size_t wasm_file_size = 565; // Set to your actual WASM binary size

    // Allocating memory for WASM file
    uint8_t *wasm_data = (uint8_t *)calloc(wasm_file_size, 1);
    if (!wasm_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for WASM file! Available heap: %" PRIu32 " bytes", esp_get_free_heap_size());
        return;
    }

    // Read WASM binary into RAM
    ESP_LOGI(TAG, "Reading full WASM file from flash");
    esp_err_t err = esp_partition_read(wasm_partition, 0, wasm_data, wasm_file_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WASM file from flash! Error: %s", esp_err_to_name(err));
        free(wasm_data);
        return;
    }

    // Validate WASM file header
    ESP_LOGI(TAG, "Checking WASM file header...");
    for (int i = 0; i < 32; i++) {
        printf("%02X ", wasm_data[i]);
    }
    printf("\n");
    
    if (!(wasm_data[0] == 0x00 && wasm_data[1] == 0x61 && wasm_data[2] == 0x73 && wasm_data[3] == 0x6D)) {
        ESP_LOGE(TAG, "Invalid WASM header! File is corrupted.");
        free(wasm_data);
        return;
    }

    // Initialize WASM runtime
    if (!wasm_runtime_init()) {
        ESP_LOGE(TAG, "Failed to initialize WASM runtime");
        free(wasm_data);
        return;
    }

    // Load WASM module
    char error_buf[128] = {0}; // Buffer for error messages
    ESP_LOGI(TAG, "Loading WASM module");
    wasm_module_t module = wasm_runtime_load(wasm_data, wasm_file_size, error_buf, sizeof(error_buf));

    if (!module) {
        ESP_LOGE(TAG, "Failed to load WASM module! Reason: %s", error_buf);
        free(wasm_data);
        return;
    }

    // Instantiate WASM module
    ESP_LOGI(TAG, "Instantiating WASM module");
    size_t available_heap = esp_get_free_heap_size() - 2048;    char inst_error_buf[128] = {0};  // Buffer to store instantiation errors

    wasm_module_inst_t module_inst = wasm_runtime_instantiate(
        module, available_heap, WASM_STACK_SIZE, inst_error_buf, sizeof(inst_error_buf));

    if (!module_inst) {
        ESP_LOGE(TAG, "Failed to instantiate WASM module! Reason: %s", inst_error_buf);
        wasm_runtime_unload(module);
        free(wasm_data);
        return;
    }


    // Create WASM execution environment
    ESP_LOGI(TAG, "Creating WASM execution environment");
    wasm_exec_env_t exec_env = wasm_runtime_create_exec_env(module_inst, WASM_STACK_SIZE);
    if (!exec_env) {
        ESP_LOGE(TAG, "Failed to create WASM execution environment! Exception: %s", wasm_runtime_get_exception(module_inst));
        wasm_runtime_deinstantiate(module_inst);
        wasm_runtime_unload(module);
        free(wasm_data);
        return;
    }

    // Lookup WASM function "add"
    wasm_function_inst_t func = wasm_runtime_lookup_function(module_inst, "add");
    if (!func) {
        ESP_LOGE(TAG, "Function 'add' not found");
    } else {
        uint32_t argv[1]; 
        if (!wasm_runtime_call_wasm(exec_env, func, 0, argv)) {
            ESP_LOGE(TAG, "WASM function execution failed! Exception: %s", wasm_runtime_get_exception(module_inst));
        } else {
            int32_t result = (int32_t)argv[0]; // Retrieve the return value
            ESP_LOGI(TAG, "Result of add: %" PRId32, result);
        }
    }

    // Free memory and clean up
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
