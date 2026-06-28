add_library(engine_physics STATIC
    src/JoltWorld.cpp
    src/PhysicsBridge.cpp
)

target_include_directories(engine_physics
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(engine_physics
    PUBLIC
        engine_core
        engine_ecs
        Jolt
)

engine_apply_compiler_options(engine_physics)
