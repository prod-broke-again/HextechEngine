#include "engine/world/World.hpp"

namespace engine {

World::World(uint64_t seed) {
    m_deferred = std::make_unique<DeferredOps>(m_registry);
    setSeed(seed);
}

void World::setSeed(uint64_t seed) {
    m_rngs.clear();
    for (size_t i = 0; i < static_cast<size_t>(RngStream::_Count); ++i) {
        uint64_t streamSeed = seed + (static_cast<uint64_t>(i) * 0x9e3779b97f4a7c15ULL);
        m_rngs.emplace_back(streamSeed);
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

