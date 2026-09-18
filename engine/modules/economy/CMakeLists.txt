add_library(engine_economy STATIC
    src/Inventory.cpp
    src/EconomyCommands.cpp
    src/EconomyModule.cpp
)

target_include_directories(engine_economy PUBLIC include)

target_link_libraries(engine_economy PUBLIC
    engine_foundation
    engine_world
    engine_runtime
)

engine_apply_compiler_options(engine_economy)
