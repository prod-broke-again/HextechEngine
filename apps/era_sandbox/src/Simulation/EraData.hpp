#pragma once

#include "engine/foundation/StringHash.hpp"
#include "engine/modules/economy/Inventory.hpp"

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

namespace ResourceIds {
    inline constexpr StringHash None = ""_sh;
    inline constexpr StringHash Wood = "wood"_sh;
    inline constexpr StringHash Fish = "fish"_sh;
    inline constexpr StringHash Stone = "stone"_sh;
    inline constexpr StringHash Grain = "grain"_sh;
    inline constexpr StringHash Bread = "bread"_sh;
    inline constexpr StringHash Gold = "gold"_sh;
}

struct ResourceInfo {
    StringHash id = ResourceIds::None;
    std::string name;
    glm::vec3 color{1.0f};
};

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

constexpr uint8_t kBuildingFacingCount = 4;

inline uint8_t wrapBuildingFacing(int facing) {
    const int n = static_cast<int>(kBuildingFacingCount);
    return static_cast<uint8_t>(((facing % n) + n) % n);
}

enum class BuildingCategory : uint8_t {
    Anchor = 0,
    Housing,
    Gathering,
    Refinement,
    Infrastructure
};

struct NeedDef {
    StringHash resource = ResourceIds::Fish;
    float consumptionPerMinute = 0.2f; // Per full house
    bool isMandatory = true;
};

struct ProductionRecipe {
    StringHash inputResource = ResourceIds::None;
    float inputPerMinute = 0.0f;
    StringHash outputResource = ResourceIds::None;
    float outputPerMinute = 0.0f;
    float cycleSeconds = 4.0f;
};

struct BuildingDef {
    StringHash id = BuildingIds::None;
    std::string stringId = "none";
    std::string name = "None";
    BuildingCategory category = BuildingCategory::Infrastructure;
    EraType requiredEra = EraType::StoneAge;
    std::vector<engine::economy::ResourceQuantity> cost{};
    glm::ivec2 footprint{1, 1};
    int maxInhabitants = 0;
    float baseTaxIncomePerMinute = 0.0f;
    ProductionRecipe production{};
    glm::vec3 primaryColor{1.0f};
    std::string description = "";
    std::string mesh = "";
};

struct CarrierDef {
    float speed = 0.73f;  // World units per second on dirt / worn trails
    float roadSpeedMultiplier = 1.35f; // Modest boost on paved roads
    int capacity = 2;    // Cargo capacity
};

struct EvolutionRequirement {
    StringHash resource = ResourceIds::Wood;
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

std::string_view getResourceName(StringHash id);
glm::vec3 getResourceColor(StringHash id);
StringHash parseResourceId(std::string_view str);
const std::vector<ResourceInfo>& getAllResources();

std::string_view getEraName(EraType era);
std::string_view getCategoryName(BuildingCategory category);

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
