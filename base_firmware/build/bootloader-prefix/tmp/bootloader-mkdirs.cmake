# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/todhan/esp/v5.4/esp-idf/components/bootloader/subproject"
  "/home/todhan/work/esp32/embedding-wamr-partition/base_firmware/build/bootloader"
  "/home/todhan/work/esp32/embedding-wamr-partition/base_firmware/build/bootloader-prefix"
  "/home/todhan/work/esp32/embedding-wamr-partition/base_firmware/build/bootloader-prefix/tmp"
  "/home/todhan/work/esp32/embedding-wamr-partition/base_firmware/build/bootloader-prefix/src/bootloader-stamp"
  "/home/todhan/work/esp32/embedding-wamr-partition/base_firmware/build/bootloader-prefix/src"
  "/home/todhan/work/esp32/embedding-wamr-partition/base_firmware/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/todhan/work/esp32/embedding-wamr-partition/base_firmware/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/todhan/work/esp32/embedding-wamr-partition/base_firmware/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
