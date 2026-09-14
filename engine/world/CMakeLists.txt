add_library(engine_world STATIC
    src/World.cpp
)

target_include_directories(engine_world PUBLIC include)
target_link_libraries(engine_world PUBLIC engine_foundation EnTT::EnTT)

engine_apply_compiler_options(engine_world)
