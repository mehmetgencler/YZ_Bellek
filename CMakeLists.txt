cmake_minimum_required(VERSION 3.15)
project(memo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

# Ensure nlohmann/json single-header is available at third_party/nlohmann-json/single_include/nlohmann/json.hpp
# If it's not present, attempt to download it at configure time from the official upstream tag.
set(NLOHMANN_JSON_URL "https://raw.githubusercontent.com/nlohmann/json/v3.11.2/single_include/nlohmann/json.hpp")
set(NLOHMANN_JSON_LOCAL "${CMAKE_SOURCE_DIR}/third_party/nlohmann-json/single_include/nlohmann/json.hpp")
# Create directory and download if missing
if(NOT EXISTS "${NLOHMANN_JSON_LOCAL}")
    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/third_party/nlohmann-json/single_include/nlohmann")
    message(STATUS "nlohmann/json.hpp not found locally; attempting to download from ${NLOHMANN_JSON_URL}")
    file(DOWNLOAD "${NLOHMANN_JSON_URL}" "${NLOHMANN_JSON_LOCAL}" SHOW_PROGRESS STATUS download_status)
    list(GET download_status 0 download_result)
    if(NOT download_result EQUAL 0)
        message(WARNING "Failed to download nlohmann/json.hpp automatically. Please run scripts/download_json.sh or place the single-header at: ${NLOHMANN_JSON_LOCAL}")
    else()
        message(STATUS "Downloaded nlohmann/json.hpp to ${NLOHMANN_JSON_LOCAL}")
    endif()
endif()

# Add third_party include path for nlohmann::json single header
include_directories(${CMAKE_SOURCE_DIR}/third_party/nlohmann-json/single_include)

add_subdirectory(core)
add_subdirectory(src)
add_subdirectory(tests)
