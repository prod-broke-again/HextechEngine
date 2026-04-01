add_library(engine_assets STATIC
    src/AssetManager.cpp
)

target_include_directories(engine_assets
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CGLTF_FETCH_DIR}
)

target_link_libraries(engine_assets
    PUBLIC
        engine_core
        cgltf_impl
        stb_image
)

engine_apply_compiler_options(engine_assets)
