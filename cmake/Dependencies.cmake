include(FetchContent)

if(DEFINED ENV{VULKAN_SDK})
    list(APPEND CMAKE_PREFIX_PATH "$ENV{VULKAN_SDK}")
endif()

find_package(Vulkan REQUIRED)

set(FETCHCONTENT_QUIET OFF)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

# EnTT
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.14.0
)
FetchContent_MakeAvailable(entt)

# nlohmann/json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# Vulkan Memory Allocator
FetchContent_Declare(
    VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.1.0
)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

# Expose VMA include for engine targets (FetchContent lowercases name)
set(ENGINE_VMA_INCLUDE "${vulkanmemoryallocator_SOURCE_DIR}/include" CACHE PATH "VMA headers")

# Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.5
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui_lib PUBLIC glfw Vulkan::Vulkan)

# Jolt Physics (CMake project lives in Build/)
FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.2.0
    SOURCE_SUBDIR Build
)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(JoltPhysics)

# stb_image (header-only fetch)
set(STB_FETCH_DIR "${CMAKE_BINARY_DIR}/_deps/stb")
file(MAKE_DIRECTORY "${STB_FETCH_DIR}")
if(NOT EXISTS "${STB_FETCH_DIR}/stb_image.h")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"
        "${STB_FETCH_DIR}/stb_image.h"
        SHOW_PROGRESS
    )
endif()
add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE "${STB_FETCH_DIR}")

# cgltf (header + implementation)
set(CGLTF_FETCH_DIR "${CMAKE_BINARY_DIR}/_deps/cgltf")
file(MAKE_DIRECTORY "${CGLTF_FETCH_DIR}")
if(NOT EXISTS "${CGLTF_FETCH_DIR}/cgltf.h")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/jkuhlmann/cgltf/master/cgltf.h"
        "${CGLTF_FETCH_DIR}/cgltf.h"
        SHOW_PROGRESS
    )
endif()
add_library(cgltf_impl STATIC "${CMAKE_CURRENT_LIST_DIR}/../third_party/cgltf_impl.c")
target_include_directories(cgltf_impl PUBLIC "${CGLTF_FETCH_DIR}")
