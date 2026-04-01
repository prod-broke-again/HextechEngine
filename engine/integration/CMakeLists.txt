add_library(engine_integration STATIC
    src/PlatformGLFW.cpp
    src/ImGuiLayer.cpp
)

target_include_directories(engine_integration
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(engine_integration
    PUBLIC
        engine_core
        engine_vulkan
        glfw
        imgui_lib
)

engine_apply_compiler_options(engine_integration)
