cmake_minimum_required(VERSION 3.14)
project(EAGLE-logger)

set(CMAKE_CXX_STANDARD 14)

find_package(pybind11 REQUIRED)
add_executable(main main.cpp)
target_link_libraries(main pybind11::embed)
