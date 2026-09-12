add_library(engine_vfx STATIC
    src/ParticleSystem.cpp
)

target_include_directories(engine_vfx
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${ENGINE_VMA_INCLUDE}
)

target_link_libraries(engine_vfx
    PUBLIC
        engine_core
        engine_vulkan
        engine_ecs
        glm::glm
        EnTT::EnTT
        Vulkan::Vulkan
)

if(TARGET GPUOpen::VulkanMemoryAllocator)
    target_link_libraries(engine_vfx PRIVATE GPUOpen::VulkanMemoryAllocator)
elseif(TARGET VulkanMemoryAllocator)
    target_link_libraries(engine_vfx PRIVATE VulkanMemoryAllocator)
else()
    target_include_directories(engine_vfx PRIVATE ${ENGINE_VMA_INCLUDE})
endif()

engine_apply_compiler_options(engine_vfx)
