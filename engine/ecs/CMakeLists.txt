add_library(engine_ecs INTERFACE)

target_include_directories(engine_ecs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(engine_ecs INTERFACE engine_core EnTT::EnTT glm::glm)
