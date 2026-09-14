add_library(engine_foundation
    src/dummy.cpp
)

target_include_directories(engine_foundation PUBLIC include)
target_link_libraries(engine_foundation PUBLIC EnTT::EnTT)

engine_apply_compiler_options(engine_foundation)
