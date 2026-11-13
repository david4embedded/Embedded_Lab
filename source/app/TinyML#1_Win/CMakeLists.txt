cmake_minimum_required(VERSION 3.20)

project(tinyml_gesture_test LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(EI_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse-sdk")

set(EI_DSP_C_SOURCES)
foreach(directory IN ITEMS
    TransformFunctions
    CommonTables
    BasicMathFunctions
    ComplexMathFunctions
    FastMathFunctions
    SupportFunctions
    MatrixFunctions
    StatisticsFunctions
)
    file(GLOB_RECURSE DSP_SOURCES CONFIGURE_DEPENDS
        "${EI_SDK_DIR}/CMSIS/DSP/Source/${directory}/*.c"
    )
    list(APPEND EI_DSP_C_SOURCES ${DSP_SOURCES})
endforeach()

set(EI_TFLITE_CC_SOURCES)
foreach(directory IN ITEMS
    kernels
    kernels/internal
    micro/kernels
    micro
    micro/memory_planner
    core/api
)
    file(GLOB TFLITE_SOURCES CONFIGURE_DEPENDS
        "${EI_SDK_DIR}/tensorflow/lite/${directory}/*.cc"
    )
    list(APPEND EI_TFLITE_CC_SOURCES ${TFLITE_SOURCES})
endforeach()

set(EI_CXX_SOURCES)
foreach(directory IN ITEMS
    "${CMAKE_CURRENT_SOURCE_DIR}/tflite-model"
    "${EI_SDK_DIR}/dsp/kissfft"
    "${EI_SDK_DIR}/dsp/dct"
    "${EI_SDK_DIR}/porting/posix"
    "${EI_SDK_DIR}/porting/mingw32"
)
    file(GLOB DIRECTORY_CXX_SOURCES CONFIGURE_DEPENDS
        "${directory}/*.cpp"
        "${directory}/*.cc"
    )
    list(APPEND EI_CXX_SOURCES ${DIRECTORY_CXX_SOURCES})
endforeach()
list(APPEND EI_CXX_SOURCES
    "${EI_SDK_DIR}/dsp/memory.cpp"
)

add_executable(tinyml_gesture_test
    source/main.cpp
    source/serial_port.cpp
    "${EI_SDK_DIR}/tensorflow/lite/c/common.c"
    ${EI_DSP_C_SOURCES}
    ${EI_CXX_SOURCES}
    ${EI_TFLITE_CC_SOURCES}
)

target_include_directories(tinyml_gesture_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(tinyml_gesture_test PRIVATE
    TF_LITE_DISABLE_X86_NEON=1
)

if(MSVC)
    target_compile_options(tinyml_gesture_test PRIVATE /W4)
else()
    target_compile_options(tinyml_gesture_test PRIVATE
        -Wall
        -Wextra
        -Wno-strict-aliasing
    )
endif()

if(MINGW)
    target_compile_options(tinyml_gesture_test PRIVATE
        $<$<CONFIG:Release>:-Os>
        $<$<CONFIG:Release>:-DNDEBUG>
    )
    target_link_options(tinyml_gesture_test PRIVATE
        -static
        -static-libgcc
        -static-libstdc++
    )
    target_link_libraries(tinyml_gesture_test PRIVATE m)
endif()
