#include <doctest/doctest.h>

#include "engine/modules/data/DataRegistry.hpp"
#include "engine/modules/data/DataModule.hpp"
#include "engine/modules/save/SaveModule.hpp"
#include "engine/world/World.hpp"
#include "Simulation/EraData.hpp"
#include "Simulation/CityCommands.hpp"
#include "Simulation/CitySystems.hpp"
#include "Simulation/Components.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace engine;
using namespace engine::era;

TEST_CASE("DataRegistry - Syntax and Schema Validation") {
    SUBCASE("Valid JSON string parsing") {
        const std::string validJson = R"({
            "name": "TestSettlement",
            "version": 1
        })";

        auto res = DataRegistry::parseJsonString(validJson, "test.json");
        REQUIRE(res.isOk());
        CHECK(res.value()["name"] == "TestSettlement");
        CHECK(res.value()["version"] == 1);
    }

    SUBCASE("Syntax error reporting with line number") {
        // Syntax error on line 3 (missing closing quote)
        const std::string badJson = "{\n  \"name\": \"Test\",\n  \"broken: 123\n}";
        auto res = DataRegistry::parseJsonString(badJson, "bad_syntax.json");
        REQUIRE(res.isError());
        const DataError& err = res.error();
        CHECK(err.filename == "bad_syntax.json");
        CHECK(err.line >= 3);
        CHECK_FALSE(err.reason.empty());
        CHECK_FALSE(err.format().empty());
    }

    SUBCASE("Schema validation with key and line detection") {
        const std::string buildingJson = R"([
            {
                "id": "cottage",
                "name": "Small Cottage",
                "category": "InvalidCategory",
                "requiredEra": "StoneAge"
            }
        ])";

        DataRegistry registry;
        const auto tempFile = std::filesystem::temp_directory_path() / "test_schema_buildings.json";
        {
            std::ofstream out(tempFile);
            out << buildingJson;
        }

        registry.registerFile("buildings", tempFile, [](const nlohmann::json& doc, const std::string& filename, std::string_view rawContent) {
            std::vector<DataError> errors;
            for (const auto& item : doc) {
                std::string cat = item.value("category", "");
                if (cat != "Housing" && cat != "Anchor" && cat != "Gathering") {
                    int line = DataRegistry::findLineNumber(rawContent, "\"" + cat + "\"");
                    errors.push_back(DataError{
                        .filename = filename,
                        .line = line,
                        .key = "category",
                        .reason = "Invalid building category: " + cat
                    });
                }
            }
            return errors;
        });

        auto loadRes = registry.loadAll();
        REQUIRE(loadRes.isError());
        REQUIRE(loadRes.error().size() == 1);
        const auto& err = loadRes.error()[0];
        CHECK(err.filename == tempFile.string());
        CHECK(err.line >= 4);
        CHECK(err.key == "category");
        CHECK(err.reason.find("Invalid building category") != std::string::npos);

        std::filesystem::remove(tempFile);
    }
}

TEST_CASE("DataRegistry - Hot Reload and File Modification Detection") {
    DataRegistry registry;
    const auto tempFile = std::filesystem::temp_directory_path() / "test_hot_reload.json";

    // 1. Initial write
    {
        std::ofstream out(tempFile);
        out << "{\"value\": 10}";
    }

    int valueFromHandler = 0;
    registry.registerFile("test_data", tempFile, [&valueFromHandler](const nlohmann::json& doc, const std::string&, std::string_view) {
        valueFromHandler = doc.value("value", 0);
        return std::vector<DataError>{};
    });

    auto loadRes = registry.loadAll();
    REQUIRE(loadRes.isOk());
    CHECK(valueFromHandler == 10);
    CHECK(registry.getJson("test_data") != nullptr);
    CHECK((*registry.getJson("test_data"))["value"] == 10);

    // 2. Listener tracking
    bool listenerCalled = false;
    registry.addReloadListener([&listenerCalled]() {
        listenerCalled = true;
    });

    // 3. Update file content
    {
        std::ofstream out(tempFile);
        out << "{\"value\": 42}";
    }

    // Reload
    auto reloadRes = registry.reloadFile("test_data");
    REQUIRE(reloadRes.isOk());
    CHECK(valueFromHandler == 42);
    CHECK(listenerCalled);
    CHECK((*registry.getJson("test_data"))["value"] == 42);

    std::filesystem::remove(tempFile);
}

TEST_CASE("DataDriven - Adding a new building via JSON without C++ code change") {
    // We create a temporary JSON definitions directory
    const auto tempDir = std::filesystem::temp_directory_path() / "hextech_test_data";
    std::filesystem::create_directories(tempDir);

    const std::string customBuildingsJson = R"([
        {
            "id": "town_center",
            "name": "Town Center",
            "category": "Anchor",
            "requiredEra": "StoneAge",
            "cost": {}
        },
        {
            "id": "windmill",
            "name": "Grand Windmill",
            "category": "Refinement",
            "requiredEra": "StoneAge",
            "cost": {
                "Wood": 8.0
            },
            "production": {
                "inputResource": "Grain",
                "inputPerMinute": 0.0,
                "outputResource": "Bread",
                "outputPerMinute": 4.0,
                "cycleSeconds": 5.0
            },
            "description": "Flour milling windmill."
        }
    ])";

    const std::string customErasJson = R"([
        {
            "id": "StoneAge",
            "name": "Stone Age",
            "requiredPopulation": 10,
            "evolutionRequirements": [],
            "carrier": {
                "speed": 2.2,
                "roadSpeedMultiplier": 1.4,
                "capacity": 2
            },
            "carrierPoolSize": 2,
            "unlockedBuildings": ["windmill"]
        },
        {
            "id": "BronzeAge",
            "name": "Bronze Age",
            "requiredPopulation": 20,
            "evolutionRequirements": [],
            "carrier": {
                "speed": 3.0,
                "roadSpeedMultiplier": 1.4,
                "capacity": 4
            },
            "carrierPoolSize": 4,
            "unlockedBuildings": ["windmill"]
        }
    ])";

    {
        std::ofstream bOut(tempDir / "buildings.json");
        bOut << customBuildingsJson;
        std::ofstream eOut(tempDir / "eras.json");
        eOut << customErasJson;
    }

    // Load definitions from custom directory
    REQUIRE(initEraData(tempDir));

    // Check that "windmill" building exists and is accessible
    const StringHash windmillId = "windmill"_sh;
    const BuildingDef& def = getBuildingDef(windmillId);
    CHECK(def.id == windmillId);
    CHECK(def.name == "Grand Windmill");
    CHECK(def.category == BuildingCategory::Refinement);
    CHECK(def.cost.get(ResourceType::Wood) == 8.0f);
    CHECK(def.production.outputResource == ResourceType::Bread);
    CHECK(def.production.outputPerMinute == 4.0f);

    // Verify it is in unlocked buildings for Stone Age
    auto available = getAvailableBuildingsForEra(EraType::StoneAge);
    bool foundWindmill = false;
    for (StringHash b : available) {
        if (b == windmillId) foundWindmill = true;
    }
    CHECK(foundWindmill);

    // Place the new building in a World simulation
    World world;
    CitySystems::initCity(world);
    auto& state = world.resource<CityState>();
    state.storage.set(ResourceType::Wood, 50.0f);

    PlaceBuildingCmd cmd{10, 10, windmillId};
    auto valRes = validate(world, cmd);
    CHECK(valRes.ok());

    apply(world, cmd);

    const auto& grid = world.resource<GridIndex>();
    CHECK(grid.cells[10][10].type == CellType::Building);
    entt::entity ent = grid.cells[10][10].entity;
    REQUIRE(world.registry().valid(ent));

    const auto& bComp = world.registry().get<BuildingComponent>(ent);
    CHECK(bComp.type == windmillId);

    const auto* prodComp = world.registry().try_get<ProductionComponent>(ent);
    REQUIRE(prodComp != nullptr);
    CHECK(prodComp->cycleSeconds == 5.0f);

    // Restore standard data files
    std::filesystem::remove_all(tempDir);
    initEraData();
}

TEST_CASE("SaveModule - Migration of BuildingComponent from v1 enum to v2 StringHash") {
    // Old v1 component registration helper
    struct OldBuildingComponent {
        uint8_t type = 0; // old enum
    };

    TypeRegistry oldTypes;
    oldTypes.registerComponent<OldBuildingComponent>("BuildingComponent", 1)
        .field("type", &OldBuildingComponent::type);

    // Create a registry with old buildings:
    // 1: TownCenter, 2: Residence, 3: Lumberjack, 8: Road
    entt::registry regOld;
    auto e1 = regOld.create();
    regOld.emplace<OldBuildingComponent>(e1, OldBuildingComponent{1}); // TownCenter
    auto e2 = regOld.create();
    regOld.emplace<OldBuildingComponent>(e2, OldBuildingComponent{2}); // Residence
    auto e3 = regOld.create();
    regOld.emplace<OldBuildingComponent>(e3, OldBuildingComponent{3}); // Lumberjack
    auto e4 = regOld.create();
    regOld.emplace<OldBuildingComponent>(e4, OldBuildingComponent{8}); // Road

    // Save with oldTypes to JSON
    nlohmann::json jSave;
    REQUIRE(SaveModule::saveRegistryJson(jSave, regOld, oldTypes));

    // Now load with new CityTypes (v2 with migrateBuildingComponent)
    TypeRegistry newTypes;
    CitySystems::registerCityTypes(newTypes);

    entt::registry regNew;
    REQUIRE(SaveModule::loadRegistryJson(jSave, regNew, newTypes));

    // Verify all 4 entities have migrated to StringHash
    auto view = regNew.view<BuildingComponent>();
    std::vector<StringHash> loadedTypes;
    for (auto e : view) {
        loadedTypes.push_back(view.get<BuildingComponent>(e).type);
    }
    REQUIRE(loadedTypes.size() == 4);
    CHECK(std::find(loadedTypes.begin(), loadedTypes.end(), BuildingIds::TownCenter) != loadedTypes.end());
    CHECK(std::find(loadedTypes.begin(), loadedTypes.end(), BuildingIds::Residence) != loadedTypes.end());
    CHECK(std::find(loadedTypes.begin(), loadedTypes.end(), BuildingIds::Lumberjack) != loadedTypes.end());
    CHECK(std::find(loadedTypes.begin(), loadedTypes.end(), BuildingIds::Road) != loadedTypes.end());
}
