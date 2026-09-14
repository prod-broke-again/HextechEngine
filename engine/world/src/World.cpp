#include "engine/world/World.hpp"

namespace engine {

World::World() {
    m_deferred = std::make_unique<DeferredOps>(m_registry);
    for (size_t i = 0; i < static_cast<size_t>(RngStream::_Count); ++i) {
        m_rngs.emplace_back(0); // Default seed 0, will be configured properly later
    }
}

World::~World() = default;

void World::dispatchCommands() {
    auto items = m_commandQueue.extract();
    for (const auto& item : items) {
        const auto* entry = m_commands.find(item.typeId);
        if (!entry) {
            continue;
        }
        CmdResult res = entry->validate(*this, item.data.get());
        if (res.ok()) {
            entry->apply(*this, item.data.get());
        }
    }
}

} // namespace engine

