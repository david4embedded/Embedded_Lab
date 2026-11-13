CC = gcc
CXX = g++
BUILD_DIR := build
OBJECT_DIR := $(BUILD_DIR)/obj
APP := $(BUILD_DIR)/tinyml_gesture_test.exe
OBJECTS_FILE := $(BUILD_DIR)/objects.rsp

CFLAGS += -I. -Wall -Wextra -Wno-strict-aliasing -Os -DNDEBUG
CFLAGS += -DTF_LITE_DISABLE_X86_NEON=1
CXXFLAGS += -std=c++11
LDLIBS += -lm
LDFLAGS += -static -static-libgcc -static-libstdc++

C_SOURCES := \
	$(wildcard edge-impulse-sdk/CMSIS/DSP/Source/TransformFunctions/*.c) \
	$(wildcard edge-impulse-sdk/CMSIS/DSP/Source/CommonTables/*.c) \
	$(wildcard edge-impulse-sdk/CMSIS/DSP/Source/BasicMathFunctions/*.c) \
	$(wildcard edge-impulse-sdk/CMSIS/DSP/Source/ComplexMathFunctions/*.c) \
	$(wildcard edge-impulse-sdk/CMSIS/DSP/Source/FastMathFunctions/*.c) \
	$(wildcard edge-impulse-sdk/CMSIS/DSP/Source/SupportFunctions/*.c) \
	$(wildcard edge-impulse-sdk/CMSIS/DSP/Source/MatrixFunctions/*.c) \
	$(wildcard edge-impulse-sdk/CMSIS/DSP/Source/StatisticsFunctions/*.c) \
	edge-impulse-sdk/tensorflow/lite/c/common.c

CXX_SOURCES := \
	source/main.cpp \
	$(wildcard tflite-model/*.cpp) \
	$(wildcard edge-impulse-sdk/dsp/kissfft/*.cpp) \
	$(wildcard edge-impulse-sdk/dsp/dct/*.cpp) \
	edge-impulse-sdk/dsp/memory.cpp \
	$(wildcard edge-impulse-sdk/porting/posix/*.cpp) \
	$(wildcard edge-impulse-sdk/porting/posix/*.cc) \
	$(wildcard edge-impulse-sdk/porting/mingw32/*.cpp) \
	$(wildcard edge-impulse-sdk/porting/mingw32/*.cc)

CC_SOURCES := \
	$(wildcard edge-impulse-sdk/tensorflow/lite/kernels/*.cc) \
	$(wildcard edge-impulse-sdk/tensorflow/lite/kernels/internal/*.cc) \
	$(wildcard edge-impulse-sdk/tensorflow/lite/micro/kernels/*.cc) \
	$(wildcard edge-impulse-sdk/tensorflow/lite/micro/*.cc) \
	$(wildcard edge-impulse-sdk/tensorflow/lite/micro/memory_planner/*.cc) \
	$(wildcard edge-impulse-sdk/tensorflow/lite/core/api/*.cc)

C_OBJECTS := $(patsubst %.c,$(OBJECT_DIR)/%.o,$(C_SOURCES))
CXX_OBJECTS := $(patsubst %.cpp,$(OBJECT_DIR)/%.o,$(CXX_SOURCES))
CC_OBJECTS := $(patsubst %.cc,$(OBJECT_DIR)/%.o,$(CC_SOURCES))
OBJECTS := $(C_OBJECTS) $(CXX_OBJECTS) $(CC_OBJECTS)

.PHONY: all clean
all: $(APP)

$(APP): Makefile $(OBJECTS)
	$(file >$(OBJECTS_FILE),$(OBJECTS))
	$(CXX) @$(OBJECTS_FILE) -o $@ $(LDFLAGS) $(LDLIBS)

$(OBJECT_DIR)/%.o: %.c
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJECT_DIR)/%.o: %.cc
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
	$(CXX) $(CFLAGS) $(CXXFLAGS) -c $< -o $@

$(OBJECT_DIR)/%.o: %.cpp
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
	$(CXX) $(CFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	if exist "$(BUILD_DIR)" rmdir /S /Q "$(BUILD_DIR)"
