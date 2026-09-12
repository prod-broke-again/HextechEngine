add_library(engine_audio STATIC
    src/AudioEngine.cpp
)

target_include_directories(engine_audio
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${MINIAUDIO_FETCH_DIR}
)

target_link_libraries(engine_audio
    PUBLIC
        engine_core
        engine_ecs
        miniaudio_impl
        glm::glm
)

engine_apply_compiler_options(engine_audio)
