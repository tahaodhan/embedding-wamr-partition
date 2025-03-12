#include "esp_log.h"
#include "wasm_runner.h"

void app_main(void) {
    ESP_LOGI("BASE_FW", "Booting Base Firmware");
    run_wasm_app();
}
