//flashing the wasm file into the location 
esptool.py --chip esp32c6 -p /dev/ttyUSB0 write_flash 0x6A0000 my_program.wasm

//reading the flash

esptool.py --chip esp32c6 -p /dev/ttyUSB0 read_flash 0x6A0000 0x10000 wasm_flash_dump.bin
xxd wasm_flash_dump.bin | head -n 50


//flashing the basefirmware

idf.py menuconfig
edit to custom partitions.csv

idf.py fullclean
idf.py partition-table
idf.py partition-table-flash

idf.py fullclean
idf.py reconfigure
idf.py build
idf.py flash
idf.py monitor
