add_library(era_simulation STATIC
    src/Simulation/EraData.cpp
    src/Simulation/CitySystems.cpp
    src/Simulation/CityCommands.cpp
)

target_link_libraries(era_simulation PUBLIC
    engine_world
    engine_data
    engine_economy
    engine_timer
    engine_spatial
    engine_statemachine
    glm::glm
    EnTT::EnTT
)

target_include_directories(era_simulation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

engine_apply_compiler_options(era_simulation)

add_executable(era_sandbox
    src/main.cpp
    src/EraSandboxModule.cpp
    src/Camera/RtsCamera.cpp
    src/Graphics/ProceduralCityMeshes.cpp
    src/ui/TopHudPanel.cpp
    src/ui/EvolutionBannerPanel.cpp
    src/ui/BuildDockPanel.cpp
    src/ui/InspectorPanel.cpp
    src/ui/HoverTooltipPanel.cpp
)

target_link_libraries(era_sandbox PRIVATE
    era_simulation
    engine_data
    engine_core
    engine_assets
    engine_ecs
    engine_vulkan
    engine_integration
    engine_ui
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
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets/data"
        "$<TARGET_FILE_DIR:era_sandbox>/assets/data"
)
