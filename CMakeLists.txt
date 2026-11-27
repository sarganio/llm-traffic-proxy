cmake_minimum_required(VERSION 3.25)
project(llm_traffic_proxy VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" ON)
option(BUILD_TESTING "Build unit tests" ON)

find_package(Threads REQUIRED)

add_library(proxy_core STATIC
    src/socket.cpp
)

target_include_directories(proxy_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(proxy_core PUBLIC cxx_std_20)
target_compile_options(proxy_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(proxy_core PUBLIC Threads::Threads)

if(ENABLE_SANITIZERS)
    target_compile_options(proxy_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(proxy_core PUBLIC -fsanitize=address,undefined)
endif()

add_executable(llm_proxy src/main.cpp)
target_link_libraries(llm_proxy PRIVATE proxy_core)
target_compile_options(llm_proxy PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.18.0
    )
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_executable(proxy_tests tests/test_sanity.cpp)
    target_link_libraries(proxy_tests PRIVATE proxy_core GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(proxy_tests)
endif()
