#include "Simulation/EraData.hpp"
#include "engine/modules/data/DataRegistry.hpp"

#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>

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

std::optional<ResourceType> parseResourceType(std::string_view str) {
    if (str == "Wood" || str == "wood") return ResourceType::Wood;
    if (str == "Fish" || str == "fish") return ResourceType::Fish;
    if (str == "Stone" || str == "stone") return ResourceType::Stone;
    if (str == "Grain" || str == "grain") return ResourceType::Grain;
    if (str == "Bread" || str == "bread") return ResourceType::Bread;
    if (str == "Gold" || str == "gold") return ResourceType::Gold;
    return std::nullopt;
}

std::optional<EraType> parseEraType(std::string_view str) {
    if (str == "StoneAge" || str == "stone_age" || str == "Stone Age") return EraType::StoneAge;
    if (str == "BronzeAge" || str == "bronze_age" || str == "Bronze Age") return EraType::BronzeAge;
    return std::nullopt;
}

std::optional<BuildingCategory> parseBuildingCategory(std::string_view str) {
    if (str == "Anchor" || str == "anchor" || str == "Town Center") return BuildingCategory::Anchor;
    if (str == "Housing" || str == "housing") return BuildingCategory::Housing;
    if (str == "Gathering" || str == "gathering") return BuildingCategory::Gathering;
    if (str == "Refinement" || str == "refinement") return BuildingCategory::Refinement;
    if (str == "Infrastructure" || str == "infrastructure") return BuildingCategory::Infrastructure;
    return std::nullopt;
}

static std::vector<BuildingDef> makeDefaultBuildingRegistry() {
    std::vector<BuildingDef> defs;

    // None
    defs.push_back(BuildingDef{
        .id = BuildingIds::None,
        .stringId = "none",
        .name = "None"
    });

    // Town Center
    defs.push_back(BuildingDef{
        .id = BuildingIds::TownCenter,
        .stringId = "town_center",
        .name = "Town Center",
        .category = BuildingCategory::Anchor,
        .requiredEra = EraType::StoneAge,
        .cost = {},
        .footprint = {1, 1},
        .maxInhabitants = 0,
        .baseTaxIncomePerMinute = 0.0f,
        .production = {},
        .primaryColor = {1.0f, 0.45f, 0.1f},
        .description = "Heart of the settlement. Stores materials and anchors the era.",
        .mesh = "town_center"
    });

    // Residence
    {
        BuildingDef def{
            .id = BuildingIds::Residence,
            .stringId = "residence",
            .name = "Settler House",
            .category = BuildingCategory::Housing,
            .requiredEra = EraType::StoneAge,
            .cost = {},
            .footprint = {1, 1},
            .maxInhabitants = 5,
            .baseTaxIncomePerMinute = 1.0f,
            .production = {},
            .primaryColor = {0.68f, 0.48f, 0.28f},
            .description = "Shelter for settlers. Consumes fish and firewood; pays taxes.",
            .mesh = "residence"
        };
        def.cost.set(ResourceType::Wood, 4.0f);
        defs.push_back(def);
    }

    // Lumberjack
    {
        BuildingDef def{
            .id = BuildingIds::Lumberjack,
            .stringId = "lumberjack",
            .name = "Woodcutter",
            .category = BuildingCategory::Gathering,
            .requiredEra = EraType::StoneAge,
            .cost = {},
            .footprint = {1, 1},
            .maxInhabitants = 0,
            .baseTaxIncomePerMinute = 0.0f,
            .production = ProductionRecipe{
                .inputResource = ResourceType::Wood,
                .inputPerMinute = 0.0f,
                .outputResource = ResourceType::Wood,
                .outputPerMinute = 3.0f,
                .cycleSeconds = 5.0f
            },
            .primaryColor = {0.35f, 0.55f, 0.22f},
            .description = "Harvests timber from surrounding land. Produces +3.0 Wood/min.",
            .mesh = "lumberjack"
        };
        def.cost.set(ResourceType::Wood, 5.0f);
        defs.push_back(def);
    }

    // Fishery
    {
        BuildingDef def{
            .id = BuildingIds::Fishery,
            .stringId = "fishery",
            .name = "Fisherman's Hut",
            .category = BuildingCategory::Gathering,
            .requiredEra = EraType::StoneAge,
            .cost = {},
            .footprint = {1, 1},
            .maxInhabitants = 0,
            .baseTaxIncomePerMinute = 0.0f,
            .production = ProductionRecipe{
                .inputResource = ResourceType::Fish,
                .inputPerMinute = 0.0f,
                .outputResource = ResourceType::Fish,
                .outputPerMinute = 2.0f,
                .cycleSeconds = 6.0f
            },
            .primaryColor = {0.20f, 0.55f, 0.75f},
            .description = "Catches fish in coastal traps. Produces +2.0 Fish/min (feeds 10 huts).",
            .mesh = "fishery"
        };
        def.cost.set(ResourceType::Wood, 6.0f);
        defs.push_back(def);
    }

    // Stone Quarry
    {
        BuildingDef def{
            .id = BuildingIds::StoneQuarry,
            .stringId = "stone_quarry",
            .name = "Stone Quarry",
            .category = BuildingCategory::Gathering,
            .requiredEra = EraType::BronzeAge,
            .cost = {},
            .footprint = {1, 1},
            .maxInhabitants = 0,
            .baseTaxIncomePerMinute = 0.0f,
            .production = ProductionRecipe{
                .inputResource = ResourceType::Stone,
                .inputPerMinute = 0.0f,
                .outputResource = ResourceType::Stone,
                .outputPerMinute = 2.0f,
                .cycleSeconds = 6.0f
            },
            .primaryColor = {0.60f, 0.60f, 0.65f},
            .description = "Quarries raw stone blocks for construction. Produces +2.0 Stone/min.",
            .mesh = "stone_quarry"
        };
        def.cost.set(ResourceType::Wood, 12.0f);
        defs.push_back(def);
    }

    // Wheat Farm
    {
        BuildingDef def{
            .id = BuildingIds::WheatFarm,
            .stringId = "wheat_farm",
            .name = "Wheat Farm",
            .category = BuildingCategory::Gathering,
            .requiredEra = EraType::BronzeAge,
            .cost = {},
            .footprint = {1, 1},
            .maxInhabitants = 0,
            .baseTaxIncomePerMinute = 0.0f,
            .production = ProductionRecipe{
                .inputResource = ResourceType::Grain,
                .inputPerMinute = 0.0f,
                .outputResource = ResourceType::Grain,
                .outputPerMinute = 3.0f,
                .cycleSeconds = 5.0f
            },
            .primaryColor = {0.88f, 0.78f, 0.20f},
            .description = "Harvests wheat stalks. Produces +3.0 Grain/min.",
            .mesh = "wheat_farm"
        };
        def.cost.set(ResourceType::Wood, 10.0f);
        def.cost.set(ResourceType::Stone, 4.0f);
        defs.push_back(def);
    }

    // Bakery
    {
        BuildingDef def{
            .id = BuildingIds::Bakery,
            .stringId = "bakery",
            .name = "Bakery",
            .category = BuildingCategory::Refinement,
            .requiredEra = EraType::BronzeAge,
            .cost = {},
            .footprint = {1, 1},
            .maxInhabitants = 0,
            .baseTaxIncomePerMinute = 0.0f,
            .production = ProductionRecipe{
                .inputResource = ResourceType::Grain,
                .inputPerMinute = 2.0f,
                .outputResource = ResourceType::Bread,
                .outputPerMinute = 2.0f,
                .cycleSeconds = 6.0f
            },
            .primaryColor = {0.80f, 0.45f, 0.20f},
            .description = "Mills grain and bakes bread. Produces +2.0 Bread/min from Grain.",
            .mesh = "bakery"
        };
        def.cost.set(ResourceType::Wood, 14.0f);
        def.cost.set(ResourceType::Stone, 8.0f);
        defs.push_back(def);
    }

    // Road
    {
        BuildingDef def{
            .id = BuildingIds::Road,
            .stringId = "road",
            .name = "Trail / Road",
            .category = BuildingCategory::Infrastructure,
            .requiredEra = EraType::StoneAge,
            .cost = {},
            .footprint = {1, 1},
            .maxInhabitants = 0,
            .baseTaxIncomePerMinute = 0.0f,
            .production = {},
            .primaryColor = {0.48f, 0.38f, 0.24f},
            .description = "Path connecting settlement buildings.",
            .mesh = "road"
        };
        def.cost.set(ResourceType::Wood, 1.0f);
        defs.push_back(def);
    }

    return defs;
}

static std::array<EraDefinition, kEraCount> makeDefaultEraDefinitions() {
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
            BuildingIds::Residence,
            BuildingIds::Lumberjack,
            BuildingIds::Fishery,
            BuildingIds::Road
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
            BuildingIds::Residence,
            BuildingIds::Lumberjack,
            BuildingIds::Fishery,
            BuildingIds::Road,
            BuildingIds::StoneQuarry,
            BuildingIds::WheatFarm,
            BuildingIds::Bakery
        };
    }

    return defs;
}

static std::vector<BuildingDef> s_buildings = makeDefaultBuildingRegistry();
static std::array<EraDefinition, kEraCount> s_eras = makeDefaultEraDefinitions();
static std::filesystem::path s_activeDataDir;

const BuildingDef& getBuildingDef(StringHash id) {
    for (const auto& def : s_buildings) {
        if (def.id == id) {
            return def;
        }
    }
    static const BuildingDef kNoneDef{
        .id = BuildingIds::None,
        .stringId = "none",
        .name = "None"
    };
    return kNoneDef;
}

const std::vector<BuildingDef>& getAllBuildingDefs() {
    return s_buildings;
}

std::vector<StringHash> getAvailableBuildingsForEra(EraType era) {
    std::vector<StringHash> list;
    const uint8_t maxEraVal = static_cast<uint8_t>(era);

    for (const auto& def : s_buildings) {
        if (def.id == BuildingIds::None || def.id == BuildingIds::TownCenter) {
            continue;
        }
        if (static_cast<uint8_t>(def.requiredEra) <= maxEraVal) {
            list.push_back(def.id);
        }
    }
    return list;
}

const EraDefinition& getEraDefinition(EraType era) {
    const size_t idx = static_cast<size_t>(era);
    if (idx < s_eras.size()) {
        return s_eras[idx];
    }
    return s_eras[0];
}

CarrierDef getCarrierDefForEra(EraType era) {
    return getEraDefinition(era).carrier;
}

static std::filesystem::path resolveDataDirectory(const std::filesystem::path& hint) {
    if (!hint.empty() && std::filesystem::exists(hint)) {
        return hint;
    }
    std::vector<std::filesystem::path> candidates = {
        "assets/data",
        "../assets/data",
        "../../assets/data",
        "../../../assets/data",
        "c:/Dev/HextechEngine/assets/data"
    };
    for (const auto& p : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(p, ec) && std::filesystem::is_directory(p, ec)) {
            return p;
        }
    }
    return "";
}

bool initEraData(const std::filesystem::path& dataDir) {
    s_activeDataDir = resolveDataDirectory(dataDir);
    if (s_activeDataDir.empty()) {
        std::cout << "[EraData] assets/data directory not found, using built-in defaults.\n";
        return true;
    }
    return reloadEraData();
}

bool reloadEraData() {
    if (s_activeDataDir.empty()) {
        s_activeDataDir = resolveDataDirectory("");
        if (s_activeDataDir.empty()) return false;
    }

    const auto buildingsPath = s_activeDataDir / "buildings.json";
    const auto erasPath = s_activeDataDir / "eras.json";

    std::vector<DataError> errors;

    // 1. Load and parse buildings.json
    auto buildingsRes = DataRegistry::loadJson(buildingsPath);
    std::vector<BuildingDef> loadedBuildings;
    if (!buildingsRes) {
        errors.push_back(buildingsRes.error());
    } else {
        const auto& doc = buildingsRes.value();
        std::ifstream bStream(buildingsPath);
        std::ostringstream bss;
        bss << bStream.rdbuf();
        const std::string rawContent = bss.str();

        if (!doc.is_array()) {
            errors.push_back(DataError{
                .filename = buildingsPath.string(),
                .line = 1,
                .key = "root",
                .reason = "Expected root to be a JSON array of building definitions"
            });
        } else {
            // Keep None as index 0
            loadedBuildings.push_back(BuildingDef{
                .id = BuildingIds::None,
                .stringId = "none",
                .name = "None"
            });

            for (size_t i = 0; i < doc.size(); ++i) {
                const auto& item = doc[i];
                if (!item.is_object()) {
                    errors.push_back(DataError{
                        .filename = buildingsPath.string(),
                        .line = -1,
                        .key = "buildings[" + std::to_string(i) + "]",
                        .reason = "Expected building item to be a JSON object"
                    });
                    continue;
                }

                if (!item.contains("id") || !item["id"].is_string()) {
                    errors.push_back(DataError{
                        .filename = buildingsPath.string(),
                        .line = -1,
                        .key = "buildings[" + std::to_string(i) + "].id",
                        .reason = "Missing or invalid 'id' string"
                    });
                    continue;
                }

                std::string idStr = item["id"].get<std::string>();
                int line = DataRegistry::findLineNumber(rawContent, "\"" + idStr + "\"");

                if (!item.contains("name") || !item["name"].is_string()) {
                    errors.push_back(DataError{
                        .filename = buildingsPath.string(),
                        .line = line,
                        .key = "buildings[" + idStr + "].name",
                        .reason = "Missing or invalid 'name' string"
                    });
                    continue;
                }

                std::string catStr = item.value("category", "Infrastructure");
                auto optCat = parseBuildingCategory(catStr);
                if (!optCat) {
                    errors.push_back(DataError{
                        .filename = buildingsPath.string(),
                        .line = line,
                        .key = "buildings[" + idStr + "].category",
                        .reason = "Unknown building category: " + catStr
                    });
                    continue;
                }

                std::string eraStr = item.value("requiredEra", "StoneAge");
                auto optEra = parseEraType(eraStr);
                if (!optEra) {
                    errors.push_back(DataError{
                        .filename = buildingsPath.string(),
                        .line = line,
                        .key = "buildings[" + idStr + "].requiredEra",
                        .reason = "Unknown era type: " + eraStr
                    });
                    continue;
                }

                BuildingDef def;
                def.id = StringHash(idStr);
                def.stringId = idStr;
                def.name = item["name"].get<std::string>();
                def.category = *optCat;
                def.requiredEra = *optEra;
                def.description = item.value("description", "");
                def.mesh = item.value("mesh", idStr);
                def.maxInhabitants = item.value("maxInhabitants", 0);
                def.baseTaxIncomePerMinute = item.value("baseTaxIncomePerMinute", 0.0f);

                if (item.contains("footprint") && item["footprint"].is_array() && item["footprint"].size() >= 2) {
                    def.footprint = {item["footprint"][0].get<int>(), item["footprint"][1].get<int>()};
                }

                if (item.contains("primaryColor") && item["primaryColor"].is_array() && item["primaryColor"].size() >= 3) {
                    def.primaryColor = {
                        item["primaryColor"][0].get<float>(),
                        item["primaryColor"][1].get<float>(),
                        item["primaryColor"][2].get<float>()
                    };
                }

                if (item.contains("cost") && item["cost"].is_object()) {
                    for (auto& [resName, val] : item["cost"].items()) {
                        auto optRes = parseResourceType(resName);
                        if (!optRes) {
                            errors.push_back(DataError{
                                .filename = buildingsPath.string(),
                                .line = line,
                                .key = "buildings[" + idStr + "].cost." + resName,
                                .reason = "Unknown resource in cost: " + resName
                            });
                        } else {
                            def.cost.set(*optRes, val.get<float>());
                        }
                    }
                }

                if (item.contains("production") && item["production"].is_object()) {
                    const auto& p = item["production"];
                    std::string inResName = p.value("inputResource", "Wood");
                    std::string outResName = p.value("outputResource", "Wood");
                    auto optInRes = parseResourceType(inResName);
                    auto optOutRes = parseResourceType(outResName);
                    if (!optInRes || !optOutRes) {
                        errors.push_back(DataError{
                            .filename = buildingsPath.string(),
                            .line = line,
                            .key = "buildings[" + idStr + "].production",
                            .reason = "Invalid resource type in production recipe"
                        });
                    } else {
                        def.production.inputResource = *optInRes;
                        def.production.outputResource = *optOutRes;
                        def.production.inputPerMinute = p.value("inputPerMinute", 0.0f);
                        def.production.outputPerMinute = p.value("outputPerMinute", 0.0f);
                        def.production.cycleSeconds = p.value("cycleSeconds", 4.0f);
                    }
                }

                loadedBuildings.push_back(std::move(def));
            }
        }
    }

    // 2. Load and parse eras.json
    auto erasRes = DataRegistry::loadJson(erasPath);
    std::array<EraDefinition, kEraCount> loadedEras = makeDefaultEraDefinitions();
    if (!erasRes) {
        errors.push_back(erasRes.error());
    } else {
        const auto& doc = erasRes.value();
        std::ifstream eStream(erasPath);
        std::ostringstream ess;
        ess << eStream.rdbuf();
        const std::string rawContent = ess.str();

        if (!doc.is_array()) {
            errors.push_back(DataError{
                .filename = erasPath.string(),
                .line = 1,
                .key = "root",
                .reason = "Expected root to be a JSON array of era definitions"
            });
        } else {
            for (size_t i = 0; i < doc.size(); ++i) {
                const auto& item = doc[i];
                std::string idStr = item.value("id", "");
                int line = DataRegistry::findLineNumber(rawContent, "\"" + idStr + "\"");
                auto optEra = parseEraType(idStr);
                if (!optEra) {
                    errors.push_back(DataError{
                        .filename = erasPath.string(),
                        .line = line,
                        .key = "eras[" + std::to_string(i) + "].id",
                        .reason = "Unknown era id: " + idStr
                    });
                    continue;
                }

                EraDefinition def;
                def.era = *optEra;
                def.name = item.value("name", idStr);
                def.requiredPopulation = item.value("requiredPopulation", 0);
                def.carrierPoolSize = item.value("carrierPoolSize", 2);

                if (item.contains("carrier") && item["carrier"].is_object()) {
                    def.carrier.speed = item["carrier"].value("speed", 2.2f);
                    def.carrier.roadSpeedMultiplier = item["carrier"].value("roadSpeedMultiplier", 1.4f);
                    def.carrier.capacity = item["carrier"].value("capacity", 2);
                }

                if (item.contains("evolutionRequirements") && item["evolutionRequirements"].is_array()) {
                    for (const auto& req : item["evolutionRequirements"]) {
                        std::string resName = req.value("resource", "");
                        auto optRes = parseResourceType(resName);
                        if (!optRes) {
                            errors.push_back(DataError{
                                .filename = erasPath.string(),
                                .line = line,
                                .key = "eras[" + idStr + "].evolutionRequirements." + resName,
                                .reason = "Unknown resource: " + resName
                            });
                        } else {
                            def.evolutionRequirements.push_back(EvolutionRequirement{
                                .resource = *optRes,
                                .requiredAmount = req.value("requiredAmount", 0.0f)
                            });
                        }
                    }
                }

                if (item.contains("unlockedBuildings") && item["unlockedBuildings"].is_array()) {
                    for (const auto& ub : item["unlockedBuildings"]) {
                        if (ub.is_string()) {
                            def.unlockedBuildings.push_back(StringHash(ub.get<std::string>()));
                        }
                    }
                }

                loadedEras[static_cast<size_t>(*optEra)] = std::move(def);
            }
        }
    }

    if (!errors.empty()) {
        std::cerr << "[EraData] Errors while loading data files:\n";
        for (const auto& err : errors) {
            std::cerr << "  " << err.format() << "\n";
        }
        return false;
    }

    if (!loadedBuildings.empty()) {
        s_buildings = std::move(loadedBuildings);
    }
    s_eras = std::move(loadedEras);
    std::cout << "[EraData] Successfully loaded " << s_buildings.size() << " buildings from JSON.\n";
    return true;
}

} // namespace engine::era
