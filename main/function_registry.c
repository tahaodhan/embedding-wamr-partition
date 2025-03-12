#include "function_registry.h"
#include "wasm_export.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LOG_TAG "function_registry"

static void wasm_gpio_set_level(wasm_exec_env_t exec_env, int32_t pin, int32_t level) {
    gpio_set_level((gpio_num_t)pin, level);
}

static void wasm_sleep_ms(wasm_exec_env_t exec_env, int32_t milliseconds) {
    vTaskDelay(milliseconds / portTICK_PERIOD_MS);
}

static void wasm_print_debug(wasm_exec_env_t exec_env, const char *message) {
    ESP_LOGI(LOG_TAG, "WASM Debug: %s", message);
}

void register_functions() {
    ESP_LOGI(LOG_TAG, "Registering native functions");

    static NativeSymbol native_symbols[] = {
        {"gpio_set_level", (void*)wasm_gpio_set_level, "(ii)", NULL},
        {"sleep_ms", (void*)wasm_sleep_ms, "(i)", NULL},
        {"print_debug", (void*)wasm_print_debug, "($)", NULL}
    };

    wasm_runtime_register_natives("env", native_symbols, sizeof(native_symbols) / sizeof(NativeSymbol));
}
