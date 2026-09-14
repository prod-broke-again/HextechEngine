#include <doctest/doctest.h>

#include "engine/modules/save/SaveModule.hpp"
#include "engine/foundation/TypeRegistry.hpp"
#include "engine/world/World.hpp"

#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include <sstream>
#include <filesystem>

using namespace engine;

// ---------------------------------------------------------------------------
// Helpers: minimal component set for round-trip tests
// ---------------------------------------------------------------------------

struct Position {
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

struct Health {
    int hp    = 100;
    int maxHp = 100;
};

struct Velocity {
    float vx = 0.f;
    float vy = 0.f;
};

static TypeRegistry makeTestRegistry() {
    TypeRegistry types;
    types.registerComponent<Position>("Position", 1)
        .field("x", &Position::x)
        .field("y", &Position::y)
        .field("z", &Position::z);
    types.registerComponent<Health>("Health", 1)
        .field("hp",    &Health::hp)
        .field("maxHp", &Health::maxHp);
    return types;
}

// ---------------------------------------------------------------------------
// Binary round-trip: World save/load
// ---------------------------------------------------------------------------

TEST_CASE("SaveModule: binary round-trip preserves world seed and tick") {
    TypeRegistry types;
    World worldA(42ULL);
    worldA.advanceTick();
    worldA.advanceTick();

    std::ostringstream oss;
    REQUIRE(SaveModule::saveBinary(oss, worldA));

    World worldB(0ULL);
    std::istringstream iss(oss.str());
    REQUIRE(SaveModule::loadBinary(iss, worldB));

    CHECK(worldB.tick().index == worldA.tick().index);
}

// ---------------------------------------------------------------------------
// JSON round-trip: registry save/load
// ---------------------------------------------------------------------------

TEST_CASE("SaveModule: JSON registry round-trip preserves component data") {
    TypeRegistry types = makeTestRegistry();

    entt::registry regA;
    const auto e1 = regA.create();
    regA.emplace<Position>(e1, Position{1.f, 2.f, 3.f});
    regA.emplace<Health>(e1, Health{80, 100});

    const auto e2 = regA.create();
    regA.emplace<Position>(e2, Position{-5.f, 0.f, 10.f});

    nlohmann::json j;
    REQUIRE(SaveModule::saveRegistryJson(j, regA, types));

    entt::registry regB;
    REQUIRE(SaveModule::loadRegistryJson(j, regB, types));

    auto view = regB.view<Position>();
    int count = 0;
    float sumX = 0.f;
    for (auto ent : view) {
        const auto& pos = view.get<Position>(ent);
        sumX += pos.x;
        ++count;
    }
    CHECK(count == 2);
    CHECK(sumX == doctest::Approx(1.f + (-5.f)));

    auto hview = regB.view<Health>();
    int hcount = 0;
    for (auto ent : hview) {
        const auto& h = hview.get<Health>(ent);
        CHECK(h.hp == 80);
        CHECK(h.maxHp == 100);
        ++hcount;
    }
    CHECK(hcount == 1);
}

// ---------------------------------------------------------------------------
// Binary registry round-trip
// ---------------------------------------------------------------------------

TEST_CASE("SaveModule: binary registry round-trip preserves component data") {
    TypeRegistry types = makeTestRegistry();

    entt::registry regA;
    const auto e1 = regA.create();
    regA.emplace<Position>(e1, Position{7.f, 8.f, 9.f});
    regA.emplace<Health>(e1, Health{50, 200});

    std::ostringstream oss;
    REQUIRE(SaveModule::saveRegistryBinary(oss, regA, types));

    entt::registry regB;
    std::istringstream iss(oss.str());
    REQUIRE(SaveModule::loadRegistryBinary(iss, regB, types));

    auto view = regB.view<Position, Health>();
    int found = 0;
    for (auto ent : view) {
        const auto& p = view.get<Position>(ent);
        const auto& h = view.get<Health>(ent);
        CHECK(p.x == doctest::Approx(7.f));
        CHECK(p.y == doctest::Approx(8.f));
        CHECK(p.z == doctest::Approx(9.f));
        CHECK(h.hp == 50);
        CHECK(h.maxHp == 200);
        ++found;
    }
    CHECK(found == 1);
}

// ---------------------------------------------------------------------------
// Unregistered component: registered data saved, unknown skipped on load
// ---------------------------------------------------------------------------

TEST_CASE("SaveModule: unregistered component is silently skipped on load") {
    TypeRegistry types = makeTestRegistry(); // Position, Health only -- NOT Velocity

    entt::registry regA;
    const auto e1 = regA.create();
    regA.emplace<Position>(e1, Position{1.f, 0.f, 0.f});
    regA.emplace<Velocity>(e1, Velocity{3.f, 0.f}); // unregistered

    nlohmann::json j;
    REQUIRE(SaveModule::saveRegistryJson(j, regA, types));

    entt::registry regB;
    REQUIRE(SaveModule::loadRegistryJson(j, regB, types));

    auto view = regB.view<Position>();
    CHECK(view.begin() != view.end());

    auto vview = regB.view<Velocity>();
    CHECK(vview.begin() == vview.end());
}

// ---------------------------------------------------------------------------
// File-level scene save/load
// ---------------------------------------------------------------------------

TEST_CASE("SaveModule: saveSceneJson/loadSceneJson round-trip") {
    TypeRegistry types = makeTestRegistry();

    entt::registry regA;
    const auto e1 = regA.create();
    regA.emplace<Position>(e1, Position{10.f, 0.f, -5.f});
    regA.emplace<Health>(e1, Health{60, 100});

    const glm::vec3 sun{-0.35f, -1.f, -0.25f};
    const std::filesystem::path tmpPath =
        std::filesystem::temp_directory_path() / "savemodule_test_scene.json";

    REQUIRE(SaveModule::saveSceneJson(tmpPath, regA, types, sun));
    REQUIRE(std::filesystem::exists(tmpPath));

    entt::registry regB;
    glm::vec3 outSun{};
    REQUIRE(SaveModule::loadSceneJson(tmpPath, regB, types, outSun));

    CHECK(outSun.x == doctest::Approx(sun.x));
    CHECK(outSun.y == doctest::Approx(sun.y));
    CHECK(outSun.z == doctest::Approx(sun.z));

    auto view = regB.view<Position>();
    CHECK(view.begin() != view.end());

    std::filesystem::remove(tmpPath);
}
