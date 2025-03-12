#!/bin/bash

# Paths

export WASI_SDK_PATH="$HOME/wasi-sdk"   
export PATH="$WASI_SDK_PATH/bin:$PATH"  


WASM_FILE="my_program.wasm"
C_FILE="my_program.c"
FLASH_ADDR=0x6A0000
PORT="/dev/ttyACM0"
BINARY_FILE="wasm_flash_dump.bin"

if [ -z "$WASI_SDK_PATH" ]; then
    echo "Error: WASI_SDK_PATH is not set. Please export it."
    exit 1
fi

echo "Cleaning up old build files..."
rm -f $WASM_FILE

echo "Compiling $C_FILE to WebAssembly..."
$WASI_SDK_PATH/bin/clang -O3 \
    -z stack-size=4096 -Wl,--initial-memory=65536 \
    -o $WASM_FILE $C_FILE \
    -Wl,--export=main -Wl,--export=__main_argc_argv \
    -Wl,--export=__data_end -Wl,--export=__heap_base \
    -Wl,--strip-all,--no-entry \
    -Wl,--allow-undefined \
    -nostdlib

if [ $? -ne 0 ]; then
    echo "Compilation failed! Exiting..."
    exit 1
fi

echo "Checking for reference types in the WASM binary..."
if wasm-objdump -x $WASM_FILE | grep -q "reference-types"; then
    echo "Error: WASM file contains reference types! Fix your compilation flags."
    exit 1
else
    echo "No reference types detected."
fi

# Get actual WASM file size
WASM_SIZE=$(stat --format=%s "$WASM_FILE")
echo "WASM file size: $WASM_SIZE bytes"

echo "Flashing $WASM_FILE to ESP32-C6..."
esptool.py --chip esp32c6 --port $PORT write_flash $FLASH_ADDR $WASM_FILE

# Verify flashing success
if [ $? -eq 0 ]; then
    echo "Flashing successful!"
else
    echo "Flashing failed!"
    exit 1
fi

# Read back to verify
echo "Verifying flashed WASM file..."
esptool.py --chip esp32c6 --port $PORT read_flash $FLASH_ADDR $WASM_SIZE wasm_flash_dump.bin

cmp $WASM_FILE wasm_flash_dump.bin
if [ $? -eq 0 ]; then
    echo "Flash verification successful!"
else
    echo "Flash verification failed!"
    exit 1
fi

rm -f $BINARY_FILE

echo "Build and flashing completed successfully!"
