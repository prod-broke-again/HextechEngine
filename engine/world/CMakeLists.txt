add_library(engine_world STATIC
    src/World.cpp
    src/WorldHasher.cpp
    src/bridges/PhysicsBridge.cpp
)

target_include_directories(engine_world PUBLIC include)
target_link_libraries(engine_world PUBLIC engine_foundation engine_physics engine_ecs EnTT::EnTT)

engine_apply_compiler_options(engine_world)
