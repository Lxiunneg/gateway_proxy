# 最低CMake版本要求
cmake_minimum_required(VERSION 3.10)

# 1. 设置 C++ 标准为 20（CMake 3.12+ 支持）
set(CMAKE_CXX_STANDARD 20)

# 项目名称
project(Port_Scanner_Test)

add_executable(Port_Scanner_Test ../port_scanner.h ../port_scanner.cpp ../../module/simple_log/simple_log.cpp test.cpp)

target_include_directories(Port_Scanner_Test PRIVATE
    ../module
)