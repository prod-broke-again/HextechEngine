add_library(engine_timer STATIC
    src/TickTimer.cpp
    src/TimerModule.cpp
)

target_include_directories(engine_timer PUBLIC include)

target_link_libraries(engine_timer PUBLIC
    engine_foundation
    engine_world
    engine_runtime
)

engine_apply_compiler_options(engine_timer)
