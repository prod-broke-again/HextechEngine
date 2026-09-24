#pragma once

#include "Simulation/EraData.hpp"
#include "engine/world/CmdResult.hpp"
#include "engine/world/CommandRegistry.hpp"

namespace engine {
class World;
}

namespace engine::era {

struct PlaceBuildingCmd {
    int x = 0;
    int z = 0;
    StringHash type = BuildingIds::None;
    uint8_t facing = 0; // 0..3, 90-degree steps
};

CmdResult validate(const World& world, const PlaceBuildingCmd& cmd);
void apply(World& world, const PlaceBuildingCmd& cmd);

struct DemolishBuildingCmd {
    int x = 0;
    int z = 0;
};

CmdResult validate(const World& world, const DemolishBuildingCmd& cmd);
void apply(World& world, const DemolishBuildingCmd& cmd);

struct EvolveEraCmd {
};

CmdResult validate(const World& world, const EvolveEraCmd& cmd);
void apply(World& world, const EvolveEraCmd& cmd);

struct ReloadDataCmd {
};

CmdResult validate(const World& world, const ReloadDataCmd& cmd);
void apply(World& world, const ReloadDataCmd& cmd);

void registerCityCommands(CommandRegistry& registry);

} // namespace engine::era
