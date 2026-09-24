include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
)
FetchContent_GetProperties(doctest)
if(NOT doctest_POPULATED)
    FetchContent_Populate(doctest)
endif()
add_library(doctest_lib INTERFACE)
target_include_directories(doctest_lib INTERFACE ${doctest_SOURCE_DIR})

add_executable(engine_tests
    main.cpp
    FoundationTests.cpp
    runtime/ModuleRegistry_test.cpp
    world/CommandRegistry_test.cpp
    simulation/CityCommands_test.cpp
    simulation/Pathfinder_test.cpp
    determinism/Determinism_test.cpp
    save/SaveModule_test.cpp
    data/DataModule_test.cpp
    modules/GenericModules_test.cpp
    ui/FieldAccess_test.cpp
    ui/EditorHistory_test.cpp
)

target_link_libraries(engine_tests PRIVATE
    doctest_lib
    engine_foundation
    engine_world
    engine_runtime
    engine_save
    engine_data
    engine_economy
    engine_timer
    engine_spatial
    engine_statemachine
    engine_ui
    era_simulation
    glm::glm
    EnTT::EnTT
)

engine_apply_compiler_options(engine_tests)

add_test(NAME engine_tests COMMAND engine_tests)
