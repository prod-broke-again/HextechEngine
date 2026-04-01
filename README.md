# untitled_engine

Integration-first C++ game engine (Vulkan renderer + GLFW, GLM, EnTT, Jolt, Dear ImGui, VMA, cgltf, stb_image, nlohmann/json).

## Prerequisites

- **CMake** 3.24+
- **Vulkan SDK** (Windows: install from [LunarG](https://vulkan.lunarg.com/) and set `VULKAN_SDK`)
- **C++20** compiler (MSVC 2022+ recommended on Windows)

## Build

```bash
cmake -B build -S .
cmake --build build --config Release
```

Run the sandbox:

```bash
./build/Release/sandbox.exe   # MSVC output layout may vary
```

## Layout

- `engine/core` — logging, input, events (`entt::dispatcher`), config (JSON)
- `engine/renderer/vulkan` — Vulkan context, VMA, PBR renderer scaffolding, shader hot-reload helper
- `engine/integration` — GLFW platform, ImGui Vulkan layer
- `engine/assets` — asset manager (cgltf, stb_image, async loads)
- `engine/ecs` — shared component headers
- `engine/physics` — Jolt world wrapper
- `apps/sandbox` — demo application

Optional `engine_config.json` in the working directory is loaded if present.

## Shaders

GLSL sources live under `assets/shaders/`. SPIR-V is generated at build time if `glslc` is on your `PATH` (from the Vulkan SDK).
