# Hextech Engine

An integration-first, Data-Oriented C++ game engine. Built with a focus on a custom Vulkan renderer and seamlessly connecting industry-standard open-source libraries (GLFW, GLM, EnTT, Jolt Physics, Dear ImGui, VMA, cgltf, stb_image, nlohmann/json).

## Prerequisites

- **CMake** 3.24+
- **Vulkan SDK** (Windows: install from [LunarG](https://vulkan.lunarg.com/) and set the `VULKAN_SDK` environment variable)
- **C++20** compatible compiler (MSVC 2022+ recommended on Windows, GCC/Clang on Linux)

## Build Instructions

Generate the build files and compile the project:

```bash
cmake -B build -S .
cmake --build build --config Release
```

Run the sandbox application:

```bash
./build/Release/sandbox.exe   # Note: MSVC output directory layout may vary
```

**Note:** An optional `engine_config.json` in the working directory will be loaded automatically if present.

## Project Layout

- **`engine/core`** — Core systems: logging, input abstraction, event system (`entt::dispatcher`), and config management (JSON).
- **`engine/renderer/vulkan`** — Custom Vulkan backend: swapchain context, VMA integration, PBR renderer scaffolding, and shader hot-reload capabilities.
- **`engine/integration`** — Middleware adapters: GLFW platform windowing and ImGui Vulkan layer.
- **`engine/assets`** — Asset pipeline: asynchronous loading of glTF models (cgltf) and textures (stb_image) with path-based caching.
- **`engine/ecs`** — Data-oriented design: shared EnTT component definitions.
- **`engine/physics`** — Physics simulation: Jolt Physics world wrapper and ECS bridge.
- **`apps/sandbox`** — The primary demo application, vertical slice, and testing ground.

## Shaders

GLSL source files are located under `assets/shaders/`. SPIR-V binaries (`.spv`) are generated automatically at build time if `glslc` is available on your system `PATH` (included with the Vulkan SDK).
