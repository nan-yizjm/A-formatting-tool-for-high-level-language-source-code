# 帮我写一个 CMakeLists.txt 的模板
cmake_minimum_required(VERSION 3.15)
project(CapstoneProject LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(CapstoneProject src/main.cpp src/lexer.cpp src/parser.cpp src/preprocess.cpp src/printfile.cpp src/profuction.cpp)
target_compile_definitions(CapstoneProject PRIVATE
    CAPSTONE_DEMO_INCLUDE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/demo_include")
