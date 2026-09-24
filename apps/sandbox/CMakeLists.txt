add_executable(sandbox
    src/main.cpp
    src/SandboxModule.cpp
)

target_link_libraries(sandbox PRIVATE
    engine_runtime
    engine_world
    engine_core
    engine_integration
    engine_ui
    engine_vulkan
    engine_assets
    engine_ecs
    engine_physics
    engine_audio
    engine_vfx
    engine_save
    engine_foundation
    glfw
    glm::glm
    EnTT::EnTT
)

target_include_directories(sandbox PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/sandbox/src
    ${CMAKE_SOURCE_DIR}/engine/runtime/include
    ${CMAKE_SOURCE_DIR}/engine/world/include
    ${CMAKE_SOURCE_DIR}/engine/core/include
    ${CMAKE_SOURCE_DIR}/engine/foundation/include
    ${CMAKE_SOURCE_DIR}/engine/modules/save/include
    ${CMAKE_SOURCE_DIR}/engine/integration/include
    ${CMAKE_SOURCE_DIR}/engine/renderer/vulkan/include
    ${CMAKE_SOURCE_DIR}/engine/assets/include
    ${CMAKE_SOURCE_DIR}/engine/ecs/include
    ${CMAKE_SOURCE_DIR}/engine/physics/include
    ${CMAKE_SOURCE_DIR}/engine/audio/include
    ${CMAKE_SOURCE_DIR}/engine/vfx/include
)

target_compile_definitions(sandbox PRIVATE
    SHADER_DIR="${CMAKE_SOURCE_DIR}/assets/shaders"
)

engine_apply_compiler_options(sandbox)

add_custom_command(TARGET sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets/sounds"
        "$<TARGET_FILE_DIR:sandbox>/assets/sounds"
)
