# This file is included via add_subdirectory, so there is no need to redefine project().

# Define the source files required for the test executable.
# message_passer_test.cpp: The test file itself.
# message_passer.cpp: The code under test.
# mock_FreeRTOS.cpp: The mock file.
add_executable(
    message_passer_test
    message_passer_test.cpp
    ../../source/library/comm/message_passer.cpp
    ../mocks/mock_FreeRTOS.cpp
)

# Include the GoogleTest module to enable commands like gtest_discover_tests.
include(GoogleTest)

# Add all required include directories to this test target as PRIVATE.
# Paths are long, so they are split into multiple lines for readability.
# Paths are relative to the project root directory, starting with ../../.
target_include_directories(message_passer_test PRIVATE
    # message_passer test include path
    .

    # Common project and library include paths
    ../../source/common
    ../../source/library/RTOS
    ../../source/library/comm
    ../../app/library

    # FreeRTOS-related include paths
    ../../thirdparty/FreeRTOS/FreeRTOS
    ../../thirdparty/FreeRTOS/FreeRTOS/include
    ../../thirdparty/FreeRTOS/FreeRTOS/portable/MSVC-MingW

    # Mock file include path
    ../mocks
)

# Link GoogleTest libraries to the message_passer_test executable.
# The PRIVATE keyword ensures this dependency is only for this target.
target_link_libraries(message_passer_test PRIVATE gtest_main gmock)

# Discover and register all test cases found in the executable.
gtest_discover_tests(message_passer_test)
