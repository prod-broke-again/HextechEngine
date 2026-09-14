#pragma once

#include "engine/foundation/StringHash.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace engine::era {

enum class ResourceType : uint8_t {
    Wood = 0,
    Fish,
    Stone,
    Grain,
    Bread,
    Gold,
    Count
};

constexpr size_t kResourceCount = static_cast<size_t>(ResourceType::Count);

enum class EraType : uint8_t {
    StoneAge = 0,
    BronzeAge,
    Count
};

constexpr size_t kEraCount = static_cast<size_t>(EraType::Count);

namespace BuildingIds {
    inline constexpr StringHash None = ""_sh;
    inline constexpr StringHash TownCenter = "town_center"_sh;
    inline constexpr StringHash Residence = "residence"_sh;
    inline constexpr StringHash Lumberjack = "lumberjack"_sh;
    inline constexpr StringHash Fishery = "fishery"_sh;
    inline constexpr StringHash StoneQuarry = "stone_quarry"_sh;
    inline constexpr StringHash WheatFarm = "wheat_farm"_sh;
    inline constexpr StringHash Bakery = "bakery"_sh;
    inline constexpr StringHash Road = "road"_sh;
}

enum class BuildingCategory : uint8_t {
    Anchor = 0,
    Housing,
    Gathering,
    Refinement,
    Infrastructure
};

struct ResourceBundle {
    std::array<float, kResourceCount> amounts{};

    constexpr ResourceBundle() = default;

    float get(ResourceType type) const {
        return amounts[static_cast<size_t>(type)];
    }

    void set(ResourceType type, float val) {
        amounts[static_cast<size_t>(type)] = val;
    }

    void add(ResourceType type, float delta) {
        amounts[static_cast<size_t>(type)] += delta;
    }

    bool canAfford(const ResourceBundle& cost) const {
        for (size_t i = 0; i < kResourceCount; ++i) {
            if (amounts[i] < cost.amounts[i]) {
                return false;
            }
        }
        return true;
    }

    bool tryConsume(const ResourceBundle& cost) {
        if (!canAfford(cost)) {
            return false;
        }
        for (size_t i = 0; i < kResourceCount; ++i) {
            amounts[i] -= cost.amounts[i];
        }
        return true;
    }
};

struct NeedDef {
    ResourceType resource = ResourceType::Fish;
    float consumptionPerMinute = 0.2f; // Per full house
    bool isMandatory = true;
};

struct ProductionRecipe {
    ResourceType inputResource = ResourceType::Wood;
    float inputPerMinute = 0.0f;
    ResourceType outputResource = ResourceType::Wood;
    float outputPerMinute = 0.0f;
    float cycleSeconds = 4.0f;
};

struct BuildingDef {
    StringHash id = BuildingIds::None;
    std::string stringId = "none";
    std::string name = "None";
    BuildingCategory category = BuildingCategory::Infrastructure;
    EraType requiredEra = EraType::StoneAge;
    ResourceBundle cost{};
    glm::ivec2 footprint{1, 1};
    int maxInhabitants = 0;
    float baseTaxIncomePerMinute = 0.0f;
    ProductionRecipe production{};
    glm::vec3 primaryColor{1.0f};
    std::string description = "";
    std::string mesh = "";
};

struct CarrierDef {
    float speed = 2.2f;  // World units per second
    float roadSpeedMultiplier = 1.4f; // 40% speed boost on paved roads
    int capacity = 2;    // Cargo capacity
};

struct EvolutionRequirement {
    ResourceType resource = ResourceType::Wood;
    float requiredAmount = 0.0f;
};

struct EraDefinition {
    EraType era = EraType::StoneAge;
    std::string name = "Stone Age";
    int requiredPopulation = 12;
    std::vector<EvolutionRequirement> evolutionRequirements;
    CarrierDef carrier;
    int carrierPoolSize = 2; // Number of couriers operating from Town Center
    std::vector<StringHash> unlockedBuildings;
};

std::string_view getResourceName(ResourceType type);
glm::vec3 getResourceColor(ResourceType type);
std::string_view getEraName(EraType era);
std::string_view getCategoryName(BuildingCategory category);

std::optional<ResourceType> parseResourceType(std::string_view str);
std::optional<EraType> parseEraType(std::string_view str);
std::optional<BuildingCategory> parseBuildingCategory(std::string_view str);

const BuildingDef& getBuildingDef(StringHash id);
std::vector<StringHash> getAvailableBuildingsForEra(EraType era);
const std::vector<BuildingDef>& getAllBuildingDefs();

const EraDefinition& getEraDefinition(EraType era);
CarrierDef getCarrierDefForEra(EraType era);

bool initEraData(const std::filesystem::path& dataDir = "");
bool reloadEraData();

} // namespace engine::era
