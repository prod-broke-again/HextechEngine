add_library(engine_save STATIC
    src/SaveModule.cpp
)

target_include_directories(engine_save PUBLIC include)

target_link_libraries(engine_save PUBLIC
    engine_foundation
    engine_world
    engine_runtime
    engine_core
    nlohmann_json::nlohmann_json
    glm::glm
)

engine_apply_compiler_options(engine_save)
