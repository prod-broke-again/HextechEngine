cmake_minimum_required(VERSION 3.24)
project(untitled_engine VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerOptions.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Dependencies.cmake")

option(ENGINE_BUILD_SANDBOX "Build sandbox app" ON)
option(ENGINE_BUILD_TESTS "Build tests" OFF)

add_subdirectory(engine/core)
add_subdirectory(engine/renderer/vulkan)
add_subdirectory(engine/integration)
add_subdirectory(engine/assets)
add_subdirectory(engine/ecs)
add_subdirectory(engine/physics)
add_subdirectory(engine/audio)

if(ENGINE_BUILD_SANDBOX)
    add_subdirectory(apps/sandbox)
endif()

if(ENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
