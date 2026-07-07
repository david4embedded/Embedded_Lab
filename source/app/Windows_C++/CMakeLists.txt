cmake_minimum_required(VERSION 3.20)

project(WindowsCppApp
    VERSION 0.1.0
    DESCRIPTION "GCC-based C++ desktop app for host debugging (no hardware)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Emit compile_commands.json for IntelliSense / clangd.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Keep debug symbols useful and warnings loud on GCC.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_compile_options($<$<CONFIG:Debug>:-g3>)
    add_compile_options($<$<CONFIG:Debug>:-O0>)
endif()

add_executable(main_app
    src/main.cpp
)

target_include_directories(main_app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
