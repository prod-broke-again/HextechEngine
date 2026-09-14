#pragma once

#include "engine/runtime/IModule.hpp"
#include "engine/world/World.hpp"
#include "engine/foundation/TypeRegistry.hpp"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>
#include <glm/vec3.hpp>

namespace engine {

class SaveModule : public IModule {
public:
    SaveModule() = default;
    ~SaveModule() override = default;

    std::string_view name() const override { return "SaveModule"; }
    std::span<const std::string_view> dependsOn() const override { return {}; }

    // ------------------------------------------------------------------------
    // Binary Serialization (Primary Engine Format)
    // ------------------------------------------------------------------------
    static bool saveBinary(std::ostream& out, const World& world);
    static bool loadBinary(std::istream& in, World& world);
    static bool saveBinary(const std::filesystem::path& path, const World& world);
    static bool loadBinary(const std::filesystem::path& path, World& world);

    // ------------------------------------------------------------------------
    // JSON Serialization (World State)
    // ------------------------------------------------------------------------
    static bool saveJson(nlohmann::json& out, const World& world);
    static bool loadJson(const nlohmann::json& in, World& world);
    static bool saveJson(const std::filesystem::path& path, const World& world);
    static bool loadJson(const std::filesystem::path& path, World& world);

    // ------------------------------------------------------------------------
    // Registry Serialization (Scenes / Standalone EnTT Registry)
    // ------------------------------------------------------------------------
    static bool saveSceneJson(const std::filesystem::path& path, const entt::registry& registry,
                              const TypeRegistry& types, const glm::vec3& sunDirection);
    static bool loadSceneJson(const std::filesystem::path& path, entt::registry& registry,
                              const TypeRegistry& types, glm::vec3& outSunDirection);

    static bool saveRegistryJson(nlohmann::json& out, const entt::registry& registry,
                                 const TypeRegistry& types);
    static bool loadRegistryJson(const nlohmann::json& in, entt::registry& registry,
                                 const TypeRegistry& types);

    // ------------------------------------------------------------------------
    // Registry Serialization (Binary)
    // ------------------------------------------------------------------------
    static bool saveRegistryBinary(std::ostream& out, const entt::registry& registry,
                                   const TypeRegistry& types);
    static bool loadRegistryBinary(std::istream& in, entt::registry& registry,
                                   const TypeRegistry& types);
};

} // namespace engine
