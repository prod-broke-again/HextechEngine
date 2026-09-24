add_library(engine_ui STATIC
    src/FieldAccess.cpp
    src/ComponentInspector.cpp
    src/EditorHistory.cpp
    src/TransformGizmo.cpp
    src/EditorChrome.cpp
)

target_include_directories(engine_ui PUBLIC include)

target_link_libraries(engine_ui
    PUBLIC
        engine_foundation
    PRIVATE
        imgui_lib
)

engine_apply_compiler_options(engine_ui)
