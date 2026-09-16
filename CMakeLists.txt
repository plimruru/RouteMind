cmake_minimum_required(VERSION 3.20)

project(route_planner)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

FetchContent_MakeAvailable(json)

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

target_link_libraries(route_planner
    PRIVATE
        nlohmann_json::nlohmann_json
)