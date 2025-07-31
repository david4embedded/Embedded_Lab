# 이 파일은 add_subdirectory를 통해 포함되므로 project()를 다시 정의할 필요가 없습니다.

# 테스트 실행 파일에 필요한 소스 파일들을 정의합니다.
# message_passer_test.cpp: 테스트 파일 자체
# message_passer.cpp: 테스트 대상 코드
# mock_FreeRTOS.cpp: Mock 파일
add_executable(
    message_passer_test
    message_passer_test.cpp
    ../../source/library/comm/message_passer/message_passer.cpp
    ../mocks/mock_FreeRTOS.cpp
)

# 프로젝트에 GoogleTest 모듈을 포함시켜 gtest_discover_tests를 사용할 수 있게 합니다.
include(GoogleTest)

# 이 테스트 타겟이 필요로 하는 모든 헤더 경로를 PRIVATE로 추가합니다.
# 경로가 길므로 여러 줄로 나누어 가독성을 높였습니다.
# 경로를 ../../로 시작하여 프로젝트 루트 디렉터리 기준으로 설정합니다.
target_include_directories(message_passer_test PRIVATE
    # message_passer 테스트의 헤더 경로
    .
    # 프로젝트의 공통 헤더 및 라이브러리 헤더 경로
    ../../source/common
    ../../source/library/RTOS
    ../../source/library/comm/message_passer
    ../../app/library
    # FreeRTOS 관련 헤더 경로
    ../../thirdparty/FreeRTOS
    ../../thirdparty/FreeRTOS/include
    ../../thirdparty/FreeRTOS/portable/MSVC-MingW
    # FreeRTOS-Plus-Trace 관련 헤더 경로
    ../../thirdparty/FreeRTOS-Plus/FreeRTOS-Plus-Trace/include
    ../../thirdparty/FreeRTOS-Plus/FreeRTOS-Plus-Trace/config
    ../../thirdparty/FreeRTOS-Plus/FreeRTOS-Plus-Trace/kernelports/FreeRTOS/config
    ../../thirdparty/FreeRTOS-Plus/FreeRTOS-Plus-Trace/kernelports/FreeRTOS/include
    # Mock 파일의 경로
    ../mocks
)

# message_passer_test 실행 파일에 GoogleTest 라이브러리들을 연결합니다.
# PRIVATE 키워드를 사용하여 이 타겟에만 의존성이 적용됨을 명시합니다.
target_link_libraries(message_passer_test PRIVATE gtest_main gmock)

# 테스트 실행 파일에서 발견된 모든 테스트 케이스를 등록합니다.
gtest_discover_tests(message_passer_test)
