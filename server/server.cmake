add_executable(server
        ${CMAKE_CURRENT_LIST_DIR}/EntryPoint.cpp)
target_include_directories(server
        PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(server
        PRIVATE shared)