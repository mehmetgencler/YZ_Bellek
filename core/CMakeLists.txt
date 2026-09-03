cmake_minimum_required(VERSION 3.15)

add_library(memo_core
    cold_storage.cpp
)

target_include_directories(memo_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(memo_core PUBLIC Threads::Threads)
target_compile_features(memo_core PUBLIC cxx_std_17)
