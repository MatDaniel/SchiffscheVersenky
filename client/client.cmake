add_executable(client
        ${CMAKE_CURRENT_LIST_DIR}/EntryPoint.cpp
        ${CMAKE_CURRENT_LIST_DIR}/draw/Game.cpp
        ${CMAKE_CURRENT_LIST_DIR}/draw/Scene.cpp
        ${CMAKE_CURRENT_LIST_DIR}/draw/scenes/MenuScene.cpp)
target_include_directories(client
        PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(client
        PRIVATE glfw
        PRIVATE glm
        PRIVATE glad
        PRIVATE EnTT
        PRIVATE ImGui
        PRIVATE shared)