add_library(engine_ecs STATIC
    src/Systems.cpp
)

target_include_directories(engine_ecs
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(engine_ecs
    PUBLIC
        engine_core
        engine_assets
        EnTT::EnTT
        glm::glm
)

engine_apply_compiler_options(engine_ecs)
