add_executable(client
        ${CMAKE_CURRENT_LIST_DIR}/client.rc
        ${CMAKE_CURRENT_LIST_DIR}/EntryPoint.cpp
        ${CMAKE_CURRENT_LIST_DIR}/NetworkQueue.cpp
        ${CMAKE_CURRENT_LIST_DIR}/draw/Game.cpp
        ${CMAKE_CURRENT_LIST_DIR}/draw/Scene.cpp
        ${CMAKE_CURRENT_LIST_DIR}/draw/SceneRenderer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/draw/scenes/MenuScene.cpp
        ${CMAKE_CURRENT_LIST_DIR}/draw/scenes/FieldSetupScene.cpp
        ${CMAKE_CURRENT_LIST_DIR}/draw/res/Resource.cpp
        ${CMAKE_CURRENT_LIST_DIR}/draw/res/ShaderStages.cpp
        ${CMAKE_CURRENT_LIST_DIR}/draw/res/ShaderPipelines.cpp
        ${CMAKE_CURRENT_LIST_DIR}/draw/res/Textures.cpp
        ${CMAKE_CURRENT_LIST_DIR}/draw/res/Models.cpp
        ${CMAKE_CURRENT_LIST_DIR}/draw/res/Materials.cpp
        ${CMAKE_CURRENT_LIST_DIR}/util/imemstream.cpp
        ${CMAKE_CURRENT_LIST_DIR}/draw/gobj/Projection.cpp)
target_include_directories(client
        PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(client
        PRIVATE glfw
        PRIVATE glm
        PRIVATE glad
        PRIVATE ImGui
        PRIVATE stb
        PRIVATE tinyobjloader
        PRIVATE shared)