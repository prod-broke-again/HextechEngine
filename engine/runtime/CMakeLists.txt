add_library(engine_runtime STATIC
    src/Engine.cpp
    src/ModuleRegistry.cpp
)

target_include_directories(engine_runtime PUBLIC include)
target_link_libraries(engine_runtime PUBLIC engine_world engine_core EnTT::EnTT)

engine_apply_compiler_options(engine_runtime)

