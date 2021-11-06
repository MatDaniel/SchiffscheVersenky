add_library(shared STATIC
        ${CMAKE_CURRENT_LIST_DIR}/Foo.cpp)
target_include_directories(shared
        PUBLIC ${CMAKE_CURRENT_LIST_DIR})