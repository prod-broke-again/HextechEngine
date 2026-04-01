find_program(GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")

set(SHADER_DIR "${CMAKE_SOURCE_DIR}/assets/shaders")
set(SPV_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/spirv")

set(SPV_PBR_VERT "${SPV_OUT_DIR}/pbr.vert.spv")
set(SPV_PBR_FRAG "${SPV_OUT_DIR}/pbr.frag.spv")
set(SPV_FULLSCREEN_VERT "${SPV_OUT_DIR}/fullscreen.vert.spv")
set(SPV_EQUIRECT_FRAG "${SPV_OUT_DIR}/equirect_to_cube.frag.spv")

if(GLSLC)
    file(MAKE_DIRECTORY "${SPV_OUT_DIR}")
    add_custom_command(
        OUTPUT ${SPV_PBR_VERT}
        COMMAND ${GLSLC} -fshader-stage=vertex -o ${SPV_PBR_VERT} ${SHADER_DIR}/pbr.vert
        DEPENDS ${SHADER_DIR}/pbr.vert
        COMMENT "Compile pbr.vert"
    )
    add_custom_command(
        OUTPUT ${SPV_PBR_FRAG}
        COMMAND ${GLSLC} -fshader-stage=fragment -o ${SPV_PBR_FRAG} ${SHADER_DIR}/pbr.frag
        DEPENDS ${SHADER_DIR}/pbr.frag
        COMMENT "Compile pbr.frag"
    )
    add_custom_command(
        OUTPUT ${SPV_FULLSCREEN_VERT}
        COMMAND ${GLSLC} -fshader-stage=vertex -o ${SPV_FULLSCREEN_VERT} ${SHADER_DIR}/fullscreen.vert
        DEPENDS ${SHADER_DIR}/fullscreen.vert
        COMMENT "Compile fullscreen.vert"
    )
    add_custom_command(
        OUTPUT ${SPV_EQUIRECT_FRAG}
        COMMAND ${GLSLC} -fshader-stage=fragment -o ${SPV_EQUIRECT_FRAG} ${SHADER_DIR}/equirect_to_cube.frag
        DEPENDS ${SHADER_DIR}/equirect_to_cube.frag
        COMMENT "Compile equirect_to_cube.frag"
    )
    set(ENGINE_SHADER_SPIRV ${SPV_PBR_VERT} ${SPV_PBR_FRAG} ${SPV_FULLSCREEN_VERT} ${SPV_EQUIRECT_FRAG})
else()
    message(WARNING "glslc not found; shader SPIR-V must be provided manually in ${SPV_OUT_DIR}")
    set(ENGINE_SHADER_SPIRV "")
endif()

add_library(engine_vulkan STATIC
    src/VulkanContext.cpp
    src/VmaAllocator.cpp
    src/PbrRenderer.cpp
    src/ShaderHotReload.cpp
    ${ENGINE_SHADER_SPIRV}
)

target_include_directories(engine_vulkan
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${ENGINE_VMA_INCLUDE}
)

target_link_libraries(engine_vulkan
    PUBLIC
        engine_core
        glfw
        Vulkan::Vulkan
        glm::glm
)

if(TARGET GPUOpen::VulkanMemoryAllocator)
    target_link_libraries(engine_vulkan PRIVATE GPUOpen::VulkanMemoryAllocator)
elseif(TARGET VulkanMemoryAllocator)
    target_link_libraries(engine_vulkan PRIVATE VulkanMemoryAllocator)
else()
    target_include_directories(engine_vulkan PRIVATE ${ENGINE_VMA_INCLUDE})
endif()

target_compile_definitions(engine_vulkan PRIVATE
    SHADER_DIR="${SHADER_DIR}"
    SPV_PBR_VERT_PATH="${SPV_PBR_VERT}"
    SPV_PBR_FRAG_PATH="${SPV_PBR_FRAG}"
    SPV_FULLSCREEN_VERT_PATH="${SPV_FULLSCREEN_VERT}"
    SPV_EQUIRECT_FRAG_PATH="${SPV_EQUIRECT_FRAG}"
)

engine_apply_compiler_options(engine_vulkan)
