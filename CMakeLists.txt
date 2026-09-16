cmake_minimum_required(VERSION 3.20)

project(route_planner)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(route_planner
    cpp/src/main.cpp

    cpp/src/planner/Planner.cpp

    cpp/src/io/JsonLoader.cpp
    cpp/src/io/JsonWriter.cpp
)

target_include_directories(route_planner
    PRIVATE
        cpp/include
)