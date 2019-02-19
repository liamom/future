cmake_minimum_required(VERSION 3.13)
project(future)

set(CMAKE_CXX_STANDARD 17)

add_executable(future main.cpp ns_future_test.cc ns_future.cc)
target_include_directories(future
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/external/GSL/include
            ${CMAKE_CURRENT_LIST_DIR}/external/doctest
        )