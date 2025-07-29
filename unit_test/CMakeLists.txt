cmake_minimum_required(VERSION 3.14)
project(message_passer_test)

get_filename_component(PARENT_DIR ${CMAKE_SOURCE_DIR} DIRECTORY)

# GoogleTest requires at least C++11
set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_definitions(-DUNIT_TESTING)

set(INCLUDE_DIR 
   ${PARENT_DIR}/source/common
   ${PARENT_DIR}/source/library/RTOS
   ${PARENT_DIR}/source/library/comm/message_passer
   ${PARENT_DIR}/app/library
   ${PARENT_DIR}/library/FreeRTOS
   ${PARENT_DIR}/library/FreeRTOS/include
   ${PARENT_DIR}/library/FreeRTOS/portable/MSVC-MingW
   ${PARENT_DIR}/library/FreeRTOS-Plus/FreeRTOS-Plus-Trace/include
   ${PARENT_DIR}/library/FreeRTOS-Plus/FreeRTOS-Plus-Trace/config
   ${PARENT_DIR}/library/FreeRTOS-Plus/FreeRTOS-Plus-Trace/kernelports/FreeRTOS/config
   ${PARENT_DIR}/library/FreeRTOS-Plus/FreeRTOS-Plus-Trace/kernelports/FreeRTOS/include
   )
   
set(SOURCE_DIR ${PARENT_DIR}/source)
set(MOCK_DIR ${PARENT_DIR}/unit_test/mocks)

enable_testing()

add_executable(
	# binary
	message_passer_test

	# tests
	message_passer_test.cpp  
   
	# sources
	${SOURCE_DIR}/library/comm/message_passer/message_passer.cpp
	
	# mocks
	${MOCK_DIR}/mock_FreeRTOS.cpp
)

target_link_libraries(
	message_passer_test
	gtest_main
	gmock
)

include_directories(${INCLUDE_DIR}
					${MOCK_DIR})
					
include(GoogleTest)
		
gtest_discover_tests(message_passer_test)