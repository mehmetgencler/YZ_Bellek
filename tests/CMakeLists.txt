cmake_minimum_required(VERSION 3.15)

include(FetchContent)
FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v2.13.10
)
FetchContent_MakeAvailable(catch2)

add_executable(unit_tests
  tests/test_eviction_and_versioning.cpp
)
target_include_directories(unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/core)
target_link_libraries(unit_tests PRIVATE memo_core Threads::Threads Catch2::Catch2)
target_compile_features(unit_tests PRIVATE cxx_std_17)
add_test(NAME unit_tests COMMAND unit_tests)
