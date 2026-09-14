#include <doctest/doctest.h>
#include "engine/runtime/ModuleRegistry.hpp"
#include "engine/world/World.hpp"

using namespace engine;

class TestModule : public IModule {
public:
    TestModule(std::string_view name, std::vector<std::string_view> deps) 
        : m_name(name), m_deps(deps) {}

    std::string_view name() const override { return m_name; }
    std::span<const std::string_view> dependsOn() const override { return m_deps; }

    void onAttach(World&) override { attached = true; }
    void onDetach(World&) override { attached = false; }

    std::string_view m_name;
    std::vector<std::string_view> m_deps;
    bool attached = false;
};

TEST_CASE("ModuleRegistry: Topological Sorting") {
    ModuleRegistry registry;
    registry.addModule(std::make_unique<TestModule>("A", std::vector<std::string_view>{"B"}));
    registry.addModule(std::make_unique<TestModule>("B", std::vector<std::string_view>{"C"}));
    registry.addModule(std::make_unique<TestModule>("C", std::vector<std::string_view>{}));

    World world;
    REQUIRE(registry.build(world));

    auto& sorted = registry.sortedModules();
    REQUIRE(sorted.size() == 3);
    CHECK(sorted[0]->name() == "C");
    CHECK(sorted[1]->name() == "B");
    CHECK(sorted[2]->name() == "A");

    auto* modA = static_cast<TestModule*>(sorted[2]);
    CHECK(modA->attached == true);

    registry.shutdown(world);
    CHECK(registry.sortedModules().empty() == true);
}

TEST_CASE("ModuleRegistry: Circular Dependency") {
    ModuleRegistry registry;
    registry.addModule(std::make_unique<TestModule>("A", std::vector<std::string_view>{"B"}));
    registry.addModule(std::make_unique<TestModule>("B", std::vector<std::string_view>{"A"}));

    World world;
    REQUIRE_FALSE(registry.build(world));
}

TEST_CASE("ModuleRegistry: Missing Dependency") {
    ModuleRegistry registry;
    registry.addModule(std::make_unique<TestModule>("A", std::vector<std::string_view>{"X"}));

    World world;
    REQUIRE_FALSE(registry.build(world));
}

