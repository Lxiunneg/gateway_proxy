# 最低CMake版本要求
cmake_minimum_required(VERSION 3.10)

# 1. 设置 C++ 标准为 20（CMake 3.12+ 支持）
set(CMAKE_CXX_STANDARD 20)

# 项目名称
project(Spider_Model_Gateway)

file(GLOB_RECURSE SRC
    main.cpp
    "module/*.h"
    "module/*.hpp"
    "module/*.cpp"
    "module/*.cc"
)

# 添加可执行文件
# 语法：add_executable(可执行文件名 源文件)
add_executable(${PROJECT_NAME} ${SRC})

target_include_directories(${PROJECT_NAME} PRIVATE
    module
    third-party
)

find_package(jsoncpp CONFIG REQUIRED) # json
find_package(Boost REQUIRED COMPONENTS pfr) # 反射

target_link_libraries(${PROJECT_NAME} PRIVATE 
    JsonCpp::JsonCpp
    Boost::pfr
)