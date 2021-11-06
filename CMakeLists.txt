cmake_minimum_required(VERSION 3.13)
project(SchiffscheVersenky VERSION 1.0.0 LANGUAGES CXX)

# Configuration
set(CMAKE_CXX_STANDARD 17)

# Load dependencies
add_subdirectory(3rdparty/glfw)
set(GLAD_PROFILE core)
set(GLAD_API gl=4.6)
add_subdirectory(3rdparty/glad)
add_subdirectory(3rdparty/glm)

# Create target
add_executable(SchiffscheVersenky
        src/EntryPoint.cpp
        src/draw/Game.cpp
        src/draw/Scene.cpp)
target_include_directories(SchiffscheVersenky
        PRIVATE src)
target_link_libraries(SchiffscheVersenky
        PRIVATE glfw
        PRIVATE glm
        PRIVATE glad)