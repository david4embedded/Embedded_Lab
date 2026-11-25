# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(xiao_sense_hwcheck)

target_sources(app PRIVATE
    src/main.cpp
    src/app_shell.cpp
    src/gesture_classifier.cpp
    src/imu.cpp
    src/mic.cpp
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

# The application source explicitly uses C++23 features and interfaces.
target_compile_features(app PRIVATE cxx_std_23)
