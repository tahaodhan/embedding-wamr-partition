#!/bin/bash

# Set variables
WASM_FILE="my_program.wasm"
C_FILE="my_program.c"
FLASH_ADDR=0x6A0000
PORT="/dev/ttyACM0"
WASI_SDK_PATH="$HOME/wasi-sdk"  # Change if installed elsewhere

# Ensure WASI SDK is installed
if [ ! -d "$WASI_SDK_PATH" ]; then
    echo "WASI SDK not found at $WASI_SDK_PATH!"
    exit 1
fi

# Compile the C file into WASM
echo "Compiling $C_FILE to WebAssembly using WASI SDK..."
$WASI_SDK_PATH/bin/clang -O3 \
    -z stack-size=4096 -Wl,--initial-memory=65536 \
    -o $WASM_FILE $C_FILE \
    -Wl,--export=main -Wl,--export=__main_argc_argv \
    -Wl,--export=__data_end -Wl,--export=__heap_base \
    -Wl,--strip-all,--no-entry \
    -Wl,--allow-undefined \
    -nostdlib

if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

echo "Compilation successful! Output: $WASM_FILE"

if ! command -v esptool.py &> /dev/null
then
    echo "esptool.py not found! Install ESP-IDF and make sure it's in PATH."
    exit 1
fi

echo "Flashing $WASM_FILE to ESP32 at $FLASH_ADDR..."
esptool.py --chip esp32c6 --port $PORT write_flash $FLASH_ADDR $WASM_FILE

if [ $? -ne 0 ]; then
    echo "Flashing failed!"
    exit 1
fi

echo "✅ Flashing successful!"
