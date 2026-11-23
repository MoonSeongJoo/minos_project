cmake_minimum_required(VERSION 3.13)
project(ai_infer_tx_rpi4)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ncnn REQUIRED)

add_executable(ai_infer_tx ai_infer_tx.cpp)

target_include_directories(ai_infer_tx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ai_infer_tx PRIVATE ncnn)

target_compile_options(ai_infer_tx PRIVATE
    -O2
    -march=armv7-a
    -mfpu=neon-vfpv4
    -mfloat-abi=hard
)

target_link_options(ai_infer_tx PRIVATE -static)
