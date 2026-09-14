#include "engine/modules/data/DataModule.hpp"
#include <iostream>

namespace engine {

void DataModule::onAttach(World& world) {
    if (!world.hasResource<DataRegistry>()) {
        world.emplaceResource<DataRegistry>();
    }
}

void DataModule::onDetach(World& /*world*/) {
}

void DataModule::tick(World& world) {
    ++m_tickCounter;
    if (m_tickCounter >= m_checkIntervalTicks) {
        m_tickCounter = 0;
        if (m_registry.checkForModifications()) {
            std::cout << "[DataModule] Detected modified configuration file(s). Reloading...\n";
            auto res = m_registry.loadAll();
            if (!res) {
                std::cerr << "[DataModule] Error during hot-reloading configs:\n";
                for (const auto& err : res.error()) {
                    std::cerr << "  " << err.format() << "\n";
                }
            } else {
                std::cout << "[DataModule] Configuration reloaded successfully.\n";
            }
        }
    }
}

} // namespace engine
