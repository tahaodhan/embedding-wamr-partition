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

    //find wasm partition named wasm_bin in esp32 flash
    const esp_partition_t *wasm_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, WASM_PARTITION_NAME);

    if (!wasm_partition) {
        ESP_LOGE(TAG, "Failed to find wasm partition");
        return;
    }

    //prints where the wasm partition is stored in flash
    ESP_LOGI(TAG, "found WASM partition at offset %" PRIx32 ", size: %" PRIu32 " bytes",
             wasm_partition->address, wasm_partition->size);
    

    //allocates memory for wasm file (very unsure about this not really understanding this code)
    size_t max_wasm_size = 128 * 1024;  

    //peventing memory overflow
    size_t wasm_file_size = (wasm_partition->size > max_wasm_size) ? max_wasm_size : wasm_partition->size;

    //allocating memory for wasm file initialized to 0 (changed from malloc to calloc)
    uint8_t *wasm_data = (uint8_t *)calloc(wasm_file_size, 1);
    if (!wasm_data) {
        ESP_LOGE(TAG, "Faeiled to allocate memory for WASM file Available heap: %" PRIu32 " bytes", esp_get_free_heap_size());
        return;
    }


    //reads wasm binary into RAM
    ESP_LOGI(TAG, "reading full WASM file from flash");
    esp_err_t err = esp_partition_read(wasm_partition, 0, wasm_data, wasm_file_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to read WASM file from flash Error: %s", esp_err_to_name(err));
        free(wasm_data);
        return;
    }


    //checks to see if the wasm file is valid and prints first 32 byes for debugging
    ESP_LOGI(TAG, "checking WASM file");
    for (int i = 0; i < 32; i++) {
        printf("%02X ", wasm_data[i]);
        // if ((i + 1) % 8 == 0) printf("\n");
    }
    printf("\n");
    
    if (!(wasm_data[0] == 0x00 && wasm_data[1] == 0x61 && wasm_data[2] == 0x73 && wasm_data[3] == 0x6D)) {
        ESP_LOGE(TAG, "invalid WASM header File is corrupted.");
        free(wasm_data);
        return;
    }

    


    //initializes wasm runtime
    ESP_LOGI(TAG, "Initializing WASM runtime");
    wasm_runtime_init();

    if (!wasm_runtime_init()) {
    ESP_LOGE(TAG, "Failed to initialize WASM runtime");
    free(wasm_data);
    return;
}



    //loading wasm file into runtime
    ESP_LOGI(TAG, "Looading WASM module");
    wasm_module_t module = wasm_runtime_load(wasm_data, wasm_file_size, NULL, 0);
    if (!module) {
        ESP_LOGE(TAG, "failed to load WASM module Exception");
        free(wasm_data);
        return;
    }

    ESP_LOGI(TAG, "instantiating WASM module");
    size_t available_heap;
    available_heap = esp_get_free_heap_size() / 2;

    wasm_module_inst_t module_inst = wasm_runtime_instantiate(module, available_heap, WASM_STACK_SIZE, NULL, 0);
    if (!module_inst) {
        ESP_LOGE(TAG, "failed to instantiate WASM module %s", wasm_runtime_get_exception(module_inst));
        wasm_runtime_unload(module);
        free(wasm_data);
        return;
    }



    //creates wasm execution environment and allocates memory 
    ESP_LOGI(TAG, "Creating WASM execution environment");
    wasm_exec_env_t exec_env = wasm_runtime_create_exec_env(module_inst, WASM_STACK_SIZE);
    if (!exec_env) {
        ESP_LOGE(TAG, "Failed to create WASM execution environment Exception: %s", wasm_runtime_get_exception(module_inst));
        wasm_runtime_deinstantiate(module_inst);
        wasm_runtime_unload(module);
        free(wasm_data);
        return;
    }


    //looks for add function
    wasm_function_inst_t func = wasm_runtime_lookup_function(module_inst, "add");
    if (!func) {
        ESP_LOGE(TAG, "Function 'add' not found");
    } else {
        uint32_t argv[1]; 
        if (!wasm_runtime_call_wasm(exec_env, func, 0, argv)) {
            ESP_LOGE(TAG, "WASM function execution failed: %s", wasm_runtime_get_exception(module_inst));
        } else {
            int32_t result = (int32_t)argv[0]; // Retrieve the return value
            ESP_LOGI(TAG, "Result of add: %" PRId32, result);
        }
}


    //searching for main function if not tries to call add 
    // ESP_LOGI(TAG, "Searching for WASM function...");
    // const char *wasm_function_name = "main";
    // wasm_function_inst_t func = wasm_runtime_lookup_function(module_inst, wasm_function_name);
    // if (!func) {
    //     ESP_LOGW(TAG, "function 'main' not found trying 'add' instead.");
    //     wasm_function_name = "add";
    //     func = wasm_runtime_lookup_function(module_inst, wasm_function_name);
    // }

    // if (!func) {
    //     ESP_LOGE(TAG, "no valid WASM function found to execute");
    // } else {
    //     uint32_t args[2] = {5, 7};
    //     uint32_t results[1];  

    //     if (!wasm_runtime_call_wasm(exec_env, func, 2, args)) {
    //         ESP_LOGE(TAG, "WASM function execution failed Exception: %s", wasm_runtime_get_exception(module_inst));
    //     } else {
    //         results[0] = args[0];  
    //         ESP_LOGI(TAG, "WASM function executed successfully Result: %" PRIu32 "", results[0]);
    //     }

    //     }


    //searches for main function in wasm file


    // wasm_function_inst_t func = NULL;
    // const char *func_names[] = {"main", "start", "_start", "entry", "init"};
    // size_t num_funcs = sizeof(func_names) / sizeof(func_names[0]);

    // for (size_t i = 0; i < num_funcs; i++) {
    //     func = wasm_runtime_lookup_function(module_inst, func_names[i]);
    //     if (func) {
    //         ESP_LOGI(TAG, "Found function: %s", func_names[i]);
    //         break;
    //     }
    // }

    // if (!func) {
    //     ESP_LOGE(TAG, "No valid function found in the WASM file");
    // } else {
    //     uint32_t args[2] = {5, 7};  
    //     if (!wasm_runtime_call_wasm(exec_env, func, 2, args)) {
    //         ESP_LOGE(TAG, "WASM function execution failed Exception: %s", wasm_runtime_get_exception(module_inst));
    //     } else {
    //         ESP_LOGI(TAG, "WASM function executed successfully");
    //     }
    // }

    //frees memory 
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