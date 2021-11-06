project(ImGui LANGUAGES CXX)

message(${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_draw.cpp)

add_library(ImGui STATIC
    ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui.cpp
    ${CMAKE_CURRENT_LIST_DIR}/imgui/backends/imgui_impl_opengl3.cpp
    ${CMAKE_CURRENT_LIST_DIR}/imgui/backends/imgui_impl_glfw.cpp)
target_include_directories(ImGui
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/imgui
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/imgui/backends)
target_link_libraries(ImGui
    PUBLIC glfw)