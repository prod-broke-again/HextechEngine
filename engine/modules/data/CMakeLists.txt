add_library(engine_data STATIC
    src/DataRegistry.cpp
    src/DataModule.cpp
)

target_include_directories(engine_data PUBLIC include)

target_link_libraries(engine_data PUBLIC
    engine_foundation
    engine_world
    engine_runtime
    engine_core
    nlohmann_json::nlohmann_json
)

engine_apply_compiler_options(engine_data)
