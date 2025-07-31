# Define the minimum required CMake version.
cmake_minimum_required(VERSION 3.14)

# Define the project name for unit testing.
project(unit_testing)

# Apply C++20 standard to all test projects.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add a global compile definition for unit testing.
add_definitions(-DUNIT_TESTING)

# Enable testing functionality.
enable_testing()

# Use FetchContent to download GoogleTest and include it in the project.
# This will be reused by all test subdirectories.
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
# Setting to avoid conflicts on Windows.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Include the message_passer test suite as a subdirectory.
# This command processes the CMakeLists.txt file in the message_passer subdirectory.
add_subdirectory(message_passer)
