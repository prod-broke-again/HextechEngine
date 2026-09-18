#include "engine/modules/economy/EconomyModule.hpp"
#include "engine/world/World.hpp"
#include "engine/world/WorldHasher.hpp"

namespace engine::economy {

void EconomyModule::onAttach(World& world) {
    if (!world.hasResource<EconomyStore>()) {
        world.emplaceResource<EconomyStore>();
    }

    registerEconomyCommands(world.commands());

    world.types().registerComponent<InventoryComponent>("InventoryComponent", 1);

    WorldHasher::registerResource<EconomyStore>("EconomyStore", [](const EconomyStore& s, uint64_t& h) {
        s.globalStorage.hash(h);
    });

    WorldHasher::registerComponent<InventoryComponent>("InventoryComponent", [](const InventoryComponent& c, uint64_t& h) {
        c.inventory.hash(h);
    });
}

void EconomyModule::onDetach(World& /*world*/) {
}

} // namespace engine::economy
