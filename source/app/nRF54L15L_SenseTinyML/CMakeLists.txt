# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(xiao_sense_hwcheck)

target_sources(app PRIVATE
    src/main.cpp
    src/AppShell.cpp
    src/GestureClassifier.cpp
    src/Imu.cpp
    src/Mic.cpp
)

include(${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse-sdk/cmake/utils.cmake)
add_subdirectory(edge-impulse-sdk/cmake/zephyr)

RECURSIVE_FIND_FILE_APPEND(
    MODEL_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/tflite-model
    "*.cpp"
)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(app PRIVATE
    ${MODEL_SOURCE}
)
