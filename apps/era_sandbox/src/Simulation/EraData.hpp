#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <cstdint>
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

enum class BuildingType : uint8_t {
    None = 0,
    TownCenter,
    Residence,
    Lumberjack,
    Fishery,
    StoneQuarry,
    WheatFarm,
    Bakery,
    Road,
    Count
};

constexpr size_t kBuildingTypeCount = static_cast<size_t>(BuildingType::Count);

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
    BuildingType type = BuildingType::None;
    std::string_view name = "None";
    BuildingCategory category = BuildingCategory::Infrastructure;
    EraType requiredEra = EraType::StoneAge;
    ResourceBundle cost{};
    glm::ivec2 footprint{1, 1};
    int maxInhabitants = 0;
    float baseTaxIncomePerMinute = 0.0f;
    ProductionRecipe production{};
    glm::vec3 primaryColor{1.0f};
    std::string_view description = "";
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
    std::string_view name = "Stone Age";
    int requiredPopulation = 12;
    std::vector<EvolutionRequirement> evolutionRequirements;
    CarrierDef carrier;
    int carrierPoolSize = 2; // Number of couriers operating from Town Center
    std::vector<BuildingType> unlockedBuildings;
};

std::string_view getResourceName(ResourceType type);
glm::vec3 getResourceColor(ResourceType type);
std::string_view getEraName(EraType era);
std::string_view getCategoryName(BuildingCategory category);

const BuildingDef& getBuildingDef(BuildingType type);
std::vector<BuildingType> getAvailableBuildingsForEra(EraType era);

const EraDefinition& getEraDefinition(EraType era);
CarrierDef getCarrierDefForEra(EraType era);

} // namespace engine::era
