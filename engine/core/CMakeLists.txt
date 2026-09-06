add_library(engine_core STATIC
    src/Application.cpp
    src/Log.cpp
    src/Input.cpp
    src/InputMap.cpp
    src/Events.cpp
    src/Config.cpp
    src/Time.cpp
    src/Path.cpp
)

target_compile_definitions(engine_core PUBLIC
    ENGINE_ROOT_DIR="${CMAKE_SOURCE_DIR}"
    ENGINE_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
)

target_include_directories(engine_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(engine_core
    PUBLIC
        glm::glm
        EnTT::EnTT
        nlohmann_json::nlohmann_json
)

engine_apply_compiler_options(engine_core)
