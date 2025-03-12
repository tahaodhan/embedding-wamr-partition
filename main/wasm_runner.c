#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wasm_export.h"
#include "bh_platform.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "function_registry.h"

#define WASM_PARTITION_NAME "wasm_bin"
#define MAX_WASM_FILE_SIZE (64 * 1024)  
#define LOG_TAG "wamr"


static void *app_instance_main(wasm_module_inst_t module_inst) {
    const char *exception;
    wasm_application_execute_main(module_inst, 0, NULL);
    if ((exception = wasm_runtime_get_exception(module_inst))) {
        ESP_LOGE(LOG_TAG, "WASM Exception: %s", exception);
    }
    return NULL;
}

uint8_t *load_wasm_from_flash(size_t *wasm_file_buf_size) {
    ESP_LOGI(LOG_TAG, "searching for WASM partition");

    const esp_partition_t *wasm_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, WASM_PARTITION_NAME);

    if (!wasm_partition) {
        ESP_LOGE(LOG_TAG, "failed to find WASM partition");
        return NULL;
    }

    ESP_LOGI(LOG_TAG, "found WASM partition at offset 0x%" PRIx32 ", size: %" PRIu32 " bytes",
             wasm_partition->address, wasm_partition->size);

    // allocate buffer for max possible size
    uint8_t *wasm_data = (uint8_t *)heap_caps_malloc(MAX_WASM_FILE_SIZE, MALLOC_CAP_8BIT);
    if (!wasm_data) {
        ESP_LOGE(LOG_TAG, "memory allocation failed available heap: %" PRIu32 " bytes",
                 esp_get_free_heap_size());
        return NULL;
    }

    ESP_LOGI(LOG_TAG, "reading WASM file from flash");

    // read max possible size first
    esp_err_t err = esp_partition_read(wasm_partition, 0, wasm_data, MAX_WASM_FILE_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Failed to read WASM file error: %s", esp_err_to_name(err));
        free(wasm_data);
        return NULL;
    }

    // find actual WASM file size by looking for its valid end
    size_t actual_size = 0;
    for (size_t i = MAX_WASM_FILE_SIZE - 1; i > 0; i--) {
        if (wasm_data[i] != 0xFF){  // look for non-zero byte (valid data)
            actual_size = i + 1;
            break;
        }
    }

    *wasm_file_buf_size = actual_size;  // set detected size
    ESP_LOGI(LOG_TAG, "detected WASM file size %zu bytes", *wasm_file_buf_size);

    // Validate WASM magic header
    if (!(wasm_data[0] == 0x00 && wasm_data[1] == 0x61 &&
          wasm_data[2] == 0x73 && wasm_data[3] == 0x6D)) {
        ESP_LOGE(LOG_TAG, "invalid WASM header");
        free(wasm_data);
        return NULL;
    }

    ESP_LOGI(LOG_TAG, "WASM file looaded successfully");
    return wasm_data;
}



void *iwasm_main(void *arg) {
    (void)arg;

    size_t wasm_file_buf_size = 0;
    uint8_t *wasm_file_buf = load_wasm_from_flash(&wasm_file_buf_size);

    if (!wasm_file_buf || wasm_file_buf_size == 0) {
        ESP_LOGE(LOG_TAG, "no valid WASM file foun");
        return NULL;
    }

    wasm_module_t wasm_module = NULL;
    wasm_module_inst_t wasm_module_inst = NULL;
    char error_buf[128] = {0};
    void *ret;
    RuntimeInitArgs init_args;

    memset(&init_args, 0, sizeof(RuntimeInitArgs));
    init_args.mem_alloc_type = Alloc_With_Allocator;
    init_args.mem_alloc_option.allocator.malloc_func = (void *)os_malloc;
    init_args.mem_alloc_option.allocator.realloc_func = (void *)os_realloc;
    init_args.mem_alloc_option.allocator.free_func = (void *)os_free;

    ESP_LOGI(LOG_TAG, "initializing WASM runtime");
    if (!wasm_runtime_full_init(&init_args)) {
        ESP_LOGE(LOG_TAG, "failed to initialize WASM runtime");
        free(wasm_file_buf);
        return NULL;
    }

    ESP_LOGI(LOG_TAG, "registering native functions");
    register_functions(); // from the function_registry.c


    ESP_LOGI(LOG_TAG, "last 10 bytes of WASM file");
    for (size_t i = wasm_file_buf_size - 10; i < wasm_file_buf_size; i++) {  
        printf("%02X ", wasm_file_buf[i]);  
    }
    printf("\n");

    ESP_LOGI(LOG_TAG, "loading WASM module");
    wasm_module = wasm_runtime_load(wasm_file_buf, wasm_file_buf_size, error_buf, sizeof(error_buf));
    if (!wasm_module) {
        ESP_LOGE(LOG_TAG, "Error in wasm_runtime_load: %s", error_buf);

        ESP_LOGI(LOG_TAG, "printing first 64 bytes of WASM file");
        for (size_t i = 0; i < 32; i++) {
        }
        printf("\n");

        goto cleanup;
    }




    ESP_LOGI(LOG_TAG, "instantiating WASM runtime...");
    if (!(wasm_module_inst = wasm_runtime_instantiate(wasm_module, 64 * 1024, 128 * 1024, error_buf, sizeof(error_buf)))) {
        ESP_LOGE(LOG_TAG, "Error while instantiating: %s", error_buf);
        goto cleanup;
    }

    ESP_LOGI(LOG_TAG, "executing WASM main()");
    ret = app_instance_main(wasm_module_inst);
    assert(!ret);

    ESP_LOGI(LOG_TAG, "deinstantiating WASM runtime");
    wasm_runtime_deinstantiate(wasm_module_inst);

cleanup:
    if (wasm_module) {
        ESP_LOGI(LOG_TAG, "unloading WASM module");
        wasm_runtime_unload(wasm_module);
    }

    ESP_LOGI(LOG_TAG, "destroying WASM runtime");
    wasm_runtime_destroy();

    free(wasm_file_buf);
    return NULL;
}

void run_wasm_app() {
    pthread_t t;
    int res;

    pthread_attr_t tattr;
    pthread_attr_init(&tattr);
    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setstacksize(&tattr, 4096);

    res = pthread_create(&t, &tattr, iwasm_main, NULL);
    assert(res == 0);

    res = pthread_join(t, NULL);
    assert(res == 0);

    ESP_LOGI(LOG_TAG, "WASM execution finished");
}