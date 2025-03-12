cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(base_firmware)

set(PARTITION_TABLE_CSV ${CMAKE_SOURCE_DIR}/partitions.csv)
set(EXTRA_COMPONENT_DIRS ${CMAKE_SOURCE_DIR}/wamr)