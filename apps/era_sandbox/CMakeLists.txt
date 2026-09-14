add_executable(era_sandbox
    src/main.cpp
    src/EraSandboxModule.cpp
    src/Camera/RtsCamera.cpp
    src/Simulation/EraData.cpp
    src/Simulation/CitySystems.cpp
    src/Graphics/ProceduralCityMeshes.cpp
)

target_link_libraries(era_sandbox PRIVATE
    engine_core
    engine_assets
    engine_ecs
    engine_vulkan
    engine_integration
    engine_audio
    engine_vfx
    engine_world
    engine_runtime
    glfw
    glm::glm
    EnTT::EnTT
)

target_include_directories(era_sandbox PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/era_sandbox/src
    ${CMAKE_SOURCE_DIR}/engine/core/include
    ${CMAKE_SOURCE_DIR}/engine/integration/include
    ${CMAKE_SOURCE_DIR}/engine/renderer/vulkan/include
    ${CMAKE_SOURCE_DIR}/engine/assets/include
    ${CMAKE_SOURCE_DIR}/engine/ecs/include
    ${CMAKE_SOURCE_DIR}/engine/vfx/include
    ${CMAKE_SOURCE_DIR}/engine/audio/include
)

target_compile_definitions(era_sandbox PRIVATE
    SHADER_DIR="${CMAKE_SOURCE_DIR}/assets/shaders"
)

engine_apply_compiler_options(era_sandbox)

add_custom_command(TARGET era_sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets/sounds"
        "$<TARGET_FILE_DIR:era_sandbox>/assets/sounds"
)
