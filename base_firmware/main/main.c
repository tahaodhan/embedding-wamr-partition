#include "wasm_export.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"

#include "string.h"

#define TAG "WASM"

//partition name
#define WASM_PARTITION_NAME "wasm_bin"

#define WASM_APP_MEMORY_SIZE (1024 * 1024)
#define WASM_STACK_SIZE (32 * 1024)

//function to locate and run wasm file
void run_wasm_app() {
    ESP_LOGI(TAG, "Searching for WASM partition");

    //finding a partition named wasm_bin
    const esp_partition_t *wasm_partitesp_partition_find_first_first(
        ESP_PARTITION_TYPE_DATA, 0x40, WASM_PARTITION_NAME
    );

    //if partition not found
    if (!wasm_partition) 
    {
        ESP_LOGE(TAG, "Failed to find WebAssembly partition");
        return;
    }

    ESP_LOGI(TAG, "Found WASM partition at offset" PRIx32 ", size: %" PRIu32 " bytes",
             wasm_partition->address, wasm_partition->size);

    //buffer size for reading the wasm file
    size_t wasm_file_size = 64 * 1024;
    uint8_t wasm_data[wasm_file_size];

    ESP_LOGI(TAG, "Reading WASM file from flash ");
    
    //read the wasm binary from the flash 
    esp_err_t err = esp_partition_read(wasm_partition, 0, wasm_data, wasm_file_size);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WASM file from flash Error: %s", esp_err_to_name(err));
        return;
    }


    ESP_LOGI(TAG, "First 16 bytes of WASM file");
    
    //print the first 16 bytes of the wasm file for debugging
    for (int i = 0; i < 16; i++) 
    {
        printf("%02X ", wasm_data[i]);
    }
    printf("\n");
    //check if the wasm file starts with the magic number
    if (memcmp(wasm_data, "\x00asm", 4) != 0) 
    {
        ESP_LOGE(TAG, "Invalid WASM header ");
        return;
    }

    ESP_LOGI(TAG, "Initializing WASM runtime");

    //initialize the wasm runtime
    wasm_runtime_init();

    ESP_LOGI(TAG, "Loading WASM module");
    

    wasm_module_t module = wasm_runtime_load(wasm_data, wasm_file_size, NULL, 0);
    
    if (!module) {
        ESP_LOGE(TAG, "Failed to load WASM module");
        return;
    }

    ESP_LOGI(TAG, "Instantiating WASM module");
    
    wasm_module_inst_t module_inst = wasm_runtime_instantiate(module, WASM_APP_MEMORY_SIZE, WASM_STACK_SIZE, NULL, 0);
    
    if (!module_inst) {

        ESP_LOGE(TAG, "Failed to instantiate WASM module");

        wasm_runtime_unload(module);
        return;
    }

    ESP_LOGI(TAG, "Creating WASM execution environment");
    was _exec_env_t exec_env = wasm_runtime_create_exec_env(module_inst, WASM_STACK_SIZE);
    if (!exec_env) {
        ESP_LOGE(TAG, "Failed to create WASM execution environment");

        wasm_runtime_deinstantiate(module_inst);
        wasm_runtime_unload(module);
        return;
    }

    ESP_LOGI(TAG, "Searching for 'main' function");
    
    wasm_function_inst_t func = wasm_runtime_lookup_function(module_inst, "main");

    //locate the main function inside the wasm module
    if (!func) {
        ESP_LOGE(TAG, "WASM function 'main' not found");
    } else {
        uint32_t args[1] = {0};
        wasm_runtime_call_wasm(exec_env, func, 0, args);
        ESP_LOGI(TAG, "WASM execution finished");
    }


        // Clean up execution environment
    wasm_runtime_destroy_exec_env(exec_env);
    wasm_runtime_deinstantiate(module_inst);
    wasm_runtime_unload(module);
    wasm_runtime_destroy();
}

//main function to execute everything
void app_main(void) {
    ESP_LOGI("BASE_FW", "Booting Base Firmware");

    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 10000,
        .idle_core_mask = 1,
        .trigger_panic = false
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL);

    wasm_runtime_init();
    run_wasm_app();
    wasm_runtime_destroy();
}
