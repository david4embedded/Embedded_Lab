# CMake 버전 요구사항을 정의합니다.
cmake_minimum_required(VERSION 3.14)

# 유닛 테스트 프로젝트의 이름을 정의합니다.
project(unit_testing)

# 모든 테스트 프로젝트에 C++20 표준을 적용합니다.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 유닛 테스트를 위한 전역 컴파일 정의를 추가합니다.
add_definitions(-DUNIT_TESTING)

# 테스트 기능을 활성화합니다.
enable_testing()

# FetchContent를 사용하여 GoogleTest를 외부에서 다운로드하고 프로젝트에 포함시킵니다.
# 이는 모든 테스트 서브 디렉터리에서 재사용됩니다.
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
# Windows에서 충돌을 피하기 위한 설정입니다.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# message_passer 테스트 스위트를 서브 디렉터리로 포함시킵니다.
# 이 명령어로 인해 cmake/unit_test/message_passer/CMakeLists.txt가 처리됩니다.
add_subdirectory(message_passer)
