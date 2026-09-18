add_library(engine_statemachine STATIC
    src/StateMachineModule.cpp
)

target_include_directories(engine_statemachine PUBLIC include)

target_link_libraries(engine_statemachine PUBLIC
    engine_foundation
    engine_world
    engine_runtime
)

engine_apply_compiler_options(engine_statemachine)
