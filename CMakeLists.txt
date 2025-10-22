# 最低CMake版本要求
cmake_minimum_required(VERSION 3.10)

# 1. 设置 C++ 标准为 20（CMake 3.12+ 支持）
set(CMAKE_CXX_STANDARD 20)

# 项目名称
project(GatewayApp)

# port_scanner
file(GLOB PORT_SCANNER
    "port_scanner/*.h"
    "port_scanner/*.cpp"
)

# module
file(GLOB_RECURSE MODULE
    "module/*.h"
    "module/*.cpp"
)

add_executable(GatewayApp 
    main.cpp
    ${PORT_SCANNER} 
    ${MODULE}   
)

target_include_directories(GatewayApp PRIVATE
    module
    third-party
)

add_subdirectory(port_scanner)