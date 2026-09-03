add_executable(memo_exec src/main.cpp)
target_link_libraries(memo_exec PRIVATE memo_core Threads::Threads)
target_include_directories(memo_exec PRIVATE ${CMAKE_SOURCE_DIR}/core)
