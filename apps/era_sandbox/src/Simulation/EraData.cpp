#include "Simulation/EraData.hpp"

namespace engine::era {

std::string_view getResourceName(ResourceType type) {
    switch (type) {
        case ResourceType::Wood:  return "Wood";
        case ResourceType::Fish:  return "Fish";
        case ResourceType::Stone: return "Stone";
        case ResourceType::Grain: return "Grain";
        case ResourceType::Bread: return "Bread";
        case ResourceType::Gold:  return "Gold";
        default: return "Unknown";
    }
}

glm::vec3 getResourceColor(ResourceType type) {
    switch (type) {
        case ResourceType::Wood:  return {0.70f, 0.45f, 0.20f};
        case ResourceType::Fish:  return {0.25f, 0.70f, 0.90f};
        case ResourceType::Stone: return {0.65f, 0.65f, 0.70f};
        case ResourceType::Grain: return {0.92f, 0.82f, 0.25f};
        case ResourceType::Bread: return {0.85f, 0.55f, 0.20f};
        case ResourceType::Gold:  return {1.00f, 0.84f, 0.00f};
        default: return {1.0f, 1.0f, 1.0f};
    }
}

std::string_view getEraName(EraType era) {
    switch (era) {
        case EraType::StoneAge:  return "Stone Age";
        case EraType::BronzeAge: return "Bronze Age";
        default: return "Unknown Era";
    }
}

std::string_view getCategoryName(BuildingCategory category) {
    switch (category) {
        case BuildingCategory::Anchor:         return "Town Center";
        case BuildingCategory::Housing:        return "Housing";
        case BuildingCategory::Gathering:      return "Gathering";
        case BuildingCategory::Refinement:     return "Refinement";
        case BuildingCategory::Infrastructure: return "Infrastructure";
        default: return "Other";
    }
}

static const std::array<BuildingDef, kBuildingTypeCount> kBuildingRegistry = [] {
    std::array<BuildingDef, kBuildingTypeCount> defs{};

    // None
    defs[static_cast<size_t>(BuildingType::None)] = BuildingDef{
        .type = BuildingType::None,
        .name = "None"
    };

    // Town Center (Campfire in Stone Age / Chieftain Hall in Bronze Age)
    {
        BuildingDef& def = defs[static_cast<size_t>(BuildingType::TownCenter)];
        def.type = BuildingType::TownCenter;
        def.name = "Town Center";
        def.category = BuildingCategory::Anchor;
        def.requiredEra = EraType::StoneAge;
        def.cost = {};
        def.footprint = {1, 1};
        def.maxInhabitants = 0;
        def.baseTaxIncomePerMinute = 0.0f;
        def.production = {};
        def.primaryColor = {1.0f, 0.45f, 0.1f};
        def.description = "Heart of the settlement. Stores materials and anchors the era.";
    }

    // Residence (Primitive Hut in Stone Age / Clay Cottage in Bronze Age)
    {
        BuildingDef& def = defs[static_cast<size_t>(BuildingType::Residence)];
        def.type = BuildingType::Residence;
        def.name = "Settler House";
        def.category = BuildingCategory::Housing;
        def.requiredEra = EraType::StoneAge;
        def.cost.set(ResourceType::Wood, 4.0f);
        def.footprint = {1, 1};
        def.maxInhabitants = 5;
        def.baseTaxIncomePerMinute = 1.0f;
        def.production = {};
        def.primaryColor = {0.68f, 0.48f, 0.28f};
        def.description = "Shelter for settlers. Consumes fish and firewood; pays taxes.";
    }

    // Lumberjack
    {
        BuildingDef& def = defs[static_cast<size_t>(BuildingType::Lumberjack)];
        def.type = BuildingType::Lumberjack;
        def.name = "Woodcutter";
        def.category = BuildingCategory::Gathering;
        def.requiredEra = EraType::StoneAge;
        def.cost.set(ResourceType::Wood, 5.0f);
        def.footprint = {1, 1};
        def.maxInhabitants = 0;
        def.baseTaxIncomePerMinute = 0.0f;
        def.production = ProductionRecipe{
            .inputResource = ResourceType::Wood,
            .inputPerMinute = 0.0f,
            .outputResource = ResourceType::Wood,
            .outputPerMinute = 3.0f,
            .cycleSeconds = 5.0f
        };
        def.primaryColor = {0.35f, 0.55f, 0.22f};
        def.description = "Harvests timber from surrounding land. Produces +3.0 Wood/min.";
    }

    // Fishery
    {
        BuildingDef& def = defs[static_cast<size_t>(BuildingType::Fishery)];
        def.type = BuildingType::Fishery;
        def.name = "Fisherman's Hut";
        def.category = BuildingCategory::Gathering;
        def.requiredEra = EraType::StoneAge;
        def.cost.set(ResourceType::Wood, 6.0f);
        def.footprint = {1, 1};
        def.maxInhabitants = 0;
        def.baseTaxIncomePerMinute = 0.0f;
        def.production = ProductionRecipe{
            .inputResource = ResourceType::Fish,
            .inputPerMinute = 0.0f,
            .outputResource = ResourceType::Fish,
            .outputPerMinute = 2.0f,
            .cycleSeconds = 6.0f
        };
        def.primaryColor = {0.20f, 0.55f, 0.75f};
        def.description = "Catches fish in coastal traps. Produces +2.0 Fish/min (feeds 10 huts).";
    }

    // Stone Quarry
    {
        BuildingDef& def = defs[static_cast<size_t>(BuildingType::StoneQuarry)];
        def.type = BuildingType::StoneQuarry;
        def.name = "Stone Quarry";
        def.category = BuildingCategory::Gathering;
        def.requiredEra = EraType::BronzeAge;
        def.cost.set(ResourceType::Wood, 12.0f);
        def.footprint = {1, 1};
        def.maxInhabitants = 0;
        def.baseTaxIncomePerMinute = 0.0f;
        def.production = ProductionRecipe{
            .inputResource = ResourceType::Stone,
            .inputPerMinute = 0.0f,
            .outputResource = ResourceType::Stone,
            .outputPerMinute = 2.0f,
            .cycleSeconds = 6.0f
        };
        def.primaryColor = {0.60f, 0.60f, 0.65f};
        def.description = "Quarries raw stone blocks for construction. Produces +2.0 Stone/min.";
    }

    // Wheat Farm
    {
        BuildingDef& def = defs[static_cast<size_t>(BuildingType::WheatFarm)];
        def.type = BuildingType::WheatFarm;
        def.name = "Wheat Farm";
        def.category = BuildingCategory::Gathering;
        def.requiredEra = EraType::BronzeAge;
        def.cost.set(ResourceType::Wood, 10.0f);
        def.cost.set(ResourceType::Stone, 4.0f);
        def.footprint = {1, 1};
        def.maxInhabitants = 0;
        def.baseTaxIncomePerMinute = 0.0f;
        def.production = ProductionRecipe{
            .inputResource = ResourceType::Grain,
            .inputPerMinute = 0.0f,
            .outputResource = ResourceType::Grain,
            .outputPerMinute = 3.0f,
            .cycleSeconds = 5.0f
        };
        def.primaryColor = {0.88f, 0.78f, 0.20f};
        def.description = "Harvests wheat stalks. Produces +3.0 Grain/min.";
    }

    // Bakery
    {
        BuildingDef& def = defs[static_cast<size_t>(BuildingType::Bakery)];
        def.type = BuildingType::Bakery;
        def.name = "Bakery";
        def.category = BuildingCategory::Refinement;
        def.requiredEra = EraType::BronzeAge;
        def.cost.set(ResourceType::Wood, 14.0f);
        def.cost.set(ResourceType::Stone, 8.0f);
        def.footprint = {1, 1};
        def.maxInhabitants = 0;
        def.baseTaxIncomePerMinute = 0.0f;
        def.production = ProductionRecipe{
            .inputResource = ResourceType::Grain,
            .inputPerMinute = 2.0f,
            .outputResource = ResourceType::Bread,
            .outputPerMinute = 2.0f,
            .cycleSeconds = 6.0f
        };
        def.primaryColor = {0.80f, 0.45f, 0.20f};
        def.description = "Mills grain and bakes bread. Produces +2.0 Bread/min from Grain.";
    }

    // Road
    {
        BuildingDef& def = defs[static_cast<size_t>(BuildingType::Road)];
        def.type = BuildingType::Road;
        def.name = "Trail / Road";
        def.category = BuildingCategory::Infrastructure;
        def.requiredEra = EraType::StoneAge;
        def.cost.set(ResourceType::Wood, 1.0f);
        def.footprint = {1, 1};
        def.maxInhabitants = 0;
        def.baseTaxIncomePerMinute = 0.0f;
        def.production = {};
        def.primaryColor = {0.48f, 0.38f, 0.24f};
        def.description = "Path connecting settlement buildings.";
    }

    return defs;
}();

const BuildingDef& getBuildingDef(BuildingType type) {
    const size_t idx = static_cast<size_t>(type);
    if (idx >= kBuildingRegistry.size()) {
        return kBuildingRegistry[0];
    }
    return kBuildingRegistry[idx];
}

std::vector<BuildingType> getAvailableBuildingsForEra(EraType era) {
    std::vector<BuildingType> list;
    const uint8_t maxEraVal = static_cast<uint8_t>(era);

    for (size_t i = 1; i < kBuildingRegistry.size(); ++i) {
        const auto& def = kBuildingRegistry[i];
        if (static_cast<uint8_t>(def.requiredEra) <= maxEraVal) {
            list.push_back(def.type);
        }
    }
    return list;
}

static const std::array<EraDefinition, kEraCount> kEraDefinitions = [] {
    std::array<EraDefinition, kEraCount> defs{};

    // Stone Age
    {
        EraDefinition& def = defs[static_cast<size_t>(EraType::StoneAge)];
        def.era = EraType::StoneAge;
        def.name = "Stone Age";
        def.requiredPopulation = 12;
        def.evolutionRequirements = {
            {ResourceType::Wood, 25.0f},
            {ResourceType::Fish, 20.0f}
        };
        def.carrier = CarrierDef{
            .speed = 2.2f,
            .roadSpeedMultiplier = 1.4f,
            .capacity = 2
        };
        def.carrierPoolSize = 2;
        def.unlockedBuildings = {
            BuildingType::Residence,
            BuildingType::Lumberjack,
            BuildingType::Fishery,
            BuildingType::Road
        };
    }

    // Bronze Age
    {
        EraDefinition& def = defs[static_cast<size_t>(EraType::BronzeAge)];
        def.era = EraType::BronzeAge;
        def.name = "Bronze Age";
        def.requiredPopulation = 30;
        def.evolutionRequirements = {
            {ResourceType::Stone, 60.0f},
            {ResourceType::Bread, 40.0f}
        };
        def.carrier = CarrierDef{
            .speed = 3.4f,
            .roadSpeedMultiplier = 1.4f,
            .capacity = 4
        };
        def.carrierPoolSize = 4;
        def.unlockedBuildings = {
            BuildingType::Residence,
            BuildingType::Lumberjack,
            BuildingType::Fishery,
            BuildingType::Road,
            BuildingType::StoneQuarry,
            BuildingType::WheatFarm,
            BuildingType::Bakery
        };
    }

    return defs;
}();

const EraDefinition& getEraDefinition(EraType era) {
    const size_t idx = static_cast<size_t>(era);
    if (idx < kEraDefinitions.size()) {
        return kEraDefinitions[idx];
    }
    return kEraDefinitions[0];
}

CarrierDef getCarrierDefForEra(EraType era) {
    return getEraDefinition(era).carrier;
}

} // namespace engine::era
