project(stb LANGUAGES CXX)

add_library(stb INTERFACE)
target_include_directories(ImGui
    INTERFACE ${CMAKE_CURRENT_LIST_DIR}/stb)