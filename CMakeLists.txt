cmake_minimum_required(VERSION 3.15)
project(memo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

# Add third_party include path for nlohmann::json single header
include_directories(${CMAKE_SOURCE_DIR}/third_party/nlohmann-json/single_include)

add_subdirectory(core)
add_subdirectory(src)
add_subdirectory(tests)
