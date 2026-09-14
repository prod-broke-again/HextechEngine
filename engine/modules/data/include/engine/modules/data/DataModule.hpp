#pragma once

#include "DataRegistry.hpp"
#include "engine/runtime/IModule.hpp"
#include "engine/world/World.hpp"

namespace engine {

class DataModule : public IModule {
public:
    DataModule() = default;
    ~DataModule() override = default;

    std::string_view name() const override { return "DataModule"; }
    std::span<const std::string_view> dependsOn() const override { return {}; }

    void onAttach(World& world) override;
    void onDetach(World& world) override;
    void tick(World& world) override;

    DataRegistry& registry() { return m_registry; }
    const DataRegistry& registry() const { return m_registry; }

    Result<void, std::vector<DataError>> reloadAll() {
        return m_registry.loadAll();
    }

private:
    DataRegistry m_registry;
    uint32_t m_checkIntervalTicks = 30; // Check filesystem for updates every 30 ticks
    uint32_t m_tickCounter = 0;
};

} // namespace engine
