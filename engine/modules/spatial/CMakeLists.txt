add_library(engine_spatial STATIC
    src/SpatialModule.cpp
)

target_include_directories(engine_spatial PUBLIC include)

target_link_libraries(engine_spatial PUBLIC
    engine_foundation
    engine_world
    engine_runtime
)

engine_apply_compiler_options(engine_spatial)
