cmake_minimum_required(VERSION 3.13)
project(SchiffscheVersenky VERSION 1.0.0 LANGUAGES CXX)

# Configuration
set(CMAKE_CXX_STANDARD 20)

# Load dependencies
add_subdirectory(3rdparty/glfw)
set(GLAD_PROFILE core)
set(GLAD_API gl=4.6)
add_subdirectory(3rdparty/glad)
add_subdirectory(3rdparty/glm)
add_subdirectory(3rdparty/entt)
include(3rdparty/imgui.cmake)

# Targets
include(client/client.cmake)
include(server/server.cmake)
include(shared/shared.cmake)

if (MSVC)
  target_compile_options(server PUBLIC "/ZI")
  target_link_options(server PUBLIC "/ZI" "/INCREMENTAL")
  target_compile_options(client PUBLIC "/ZI")
  target_link_options(client PUBLIC "/ZI" "/INCREMENTAL")
endif()