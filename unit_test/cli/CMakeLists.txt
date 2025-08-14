# This file is included via add_subdirectory, so there is no need to redefine project().

# Define the source files required for the test executable.
# cli_test.cpp: The code under test.
add_executable(
    cli_test
    cli_test.cpp
)

# Include the GoogleTest module to enable commands like gtest_discover_tests.
include(GoogleTest)

# Add all required include directories to this test target as PRIVATE.
# Paths are long, so they are split into multiple lines for readability.
# Paths are relative to the project root directory, starting with ../../.
target_include_directories(cli_test PRIVATE

    # Common project and library include paths
    ../../source/common
    ../../source/library
    ../../source/library/utilities
)

# Link GoogleTest libraries to the cli_test executable.
# The PRIVATE keyword ensures this dependency is only for this target.
target_link_libraries(cli_test PRIVATE gtest_main gmock)

# Discover and register all test cases found in the executable.
gtest_discover_tests(cli_test)
