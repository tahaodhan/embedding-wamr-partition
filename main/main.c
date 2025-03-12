#include "esp_log.h"
#include "wasm_runner.h"

void app_main(void) {
    ESP_LOGI("BASE_FW", "booting Firmware");
    run_wasm_app();
}
