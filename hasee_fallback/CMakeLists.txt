
cmake_minimum_required(VERSION 3.16)

project(hasee_fallback C)

# C11 language standard
set(CMAKE_C_STANDARD 11)

set(CMAKE_C_STANDARD_REQUIRED ON)

# Strict compiler warning policy
add_compile_options(
    -Wall
    -Wextra
    -Werror
)

# Fallback core static library
add_library(
    fallback_core STATIC

    src/fallback_controller.c
)

# Header include path
target_include_directories(
    fallback_core PUBLIC

    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Math library
target_link_libraries(
    fallback_core PUBLIC m
)

# Native test executable
add_executable(
    test_fallback

    tests/test_fallback.c
)

target_link_libraries(
    test_fallback PRIVATE fallback_core
)

# Keep assertions enabled for this test.
target_compile_options(
    test_fallback PRIVATE -UNDEBUG
)

# CTest configuration
enable_testing()

add_test(
    NAME fallback_deceleration

    COMMAND test_fallback
)
