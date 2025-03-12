# EMBEDDING-WAMR-PERMANENTLY

## Overview
This project embeds **WebAssembly Micro Runtime (WAMR)** permanently on an **ESP32**. It lets you run **WASM programs** without reflashing the ESP-IDF firmware every time. Once the firmware is flashed, you only need to **change the WASM file** and flash it separately.

## Steps to Use This Project

### **Step 1: Register Functions**
Before running a WASM program, you need to **register native functions** inside `function_registry.c`.

#### Example: Adding a Custom Function
Edit `function_registry.c` and add a new function inside `register_functions()`:
```c
#include "function_registry.h"
#include "wasm_export.h"
#include "esp_log.h"

static void wasm_custom_function(wasm_exec_env_t exec_env, int32_t value) {
    ESP_LOGI("WASM", "Custom Function Called with Value: %d", value);
}

void register_functions() {
    NativeSymbol native_symbols[] = {
        {"custom_function", (void*)wasm_custom_function, "(i)", NULL},
    };
    wasm_runtime_register_natives("env", native_symbols, sizeof(native_symbols) / sizeof(NativeSymbol));
}
```
This makes `custom_function()` available in WASM.


### **Step 2: Write Your WASM Program**
Now, edit `my_program.c` and write your WASM program **using the registered functions**.

#### Example: Calling the Custom Function
```c
#include <stdint.h>

__attribute__((import_module("env"), import_name("custom_function")))
void custom_function(int value);

void main() {
    custom_function(42);
}
```

#### **Build & Flash Your WASM Program**
Once you're done editing `my_program.c`, **run the `build.sh` script** to compile and flash your WASM file.
```bash
./build.sh
```
This script will:
- **Compile** `my_program.c` into `my_program.wasm`
- **Flash** `my_program.wasm` to the ESP32
- **Verify** that the file was written correctly by reading it back and comparing sizes

If the file is corrupted, it will warn you!



### **Step 3: Build, Flash & Monitor ESP32 Firmware**
Once your WASM file is ready, you can **build and flash the ESP32 firmware** (if needed):
```bash
idf.py build
idf.py flash
idf.py monitor
```
This will:
- Build the ESP32 firmware
- Flash it onto the board
- Start monitoring the serial output


