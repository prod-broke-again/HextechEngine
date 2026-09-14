#include "engine/world/WorldHasher.hpp"
#include "engine/world/World.hpp"

#include <algorithm>

namespace engine {

std::vector<WorldHasher::ComponentEntry>& WorldHasher::componentRegistry() {
    static std::vector<ComponentEntry> s_entries;
    return s_entries;
}

std::vector<WorldHasher::ResourceEntry>& WorldHasher::resourceRegistry() {
    static std::vector<ResourceEntry> s_entries;
    return s_entries;
}

void WorldHasher::clearRegistrations() {
    componentRegistry().clear();
    resourceRegistry().clear();
}

uint64_t WorldHasher::computeHash(const World& world) {
    uint64_t hash = kFnvOffsetBasis;

    // 1. Tick index
    uint64_t tickIdx = world.tick().index;
    hashPod(hash, tickIdx);

    // 2. RNG stream states
    for (size_t stream = 0; stream < static_cast<size_t>(RngStream::_Count); ++stream) {
        const auto& rng = world.rng(static_cast<RngStream>(stream));
        uint64_t s = rng.state();
        uint64_t inc = rng.inc();
        hashPod(hash, s);
        hashPod(hash, inc);
    }

    // 3. Registered resources
    for (const auto& resEntry : resourceRegistry()) {
        resEntry.hashFn(world, hash);
    }

    // 4. Collect and sort all entities in strictly ascending order
    std::vector<entt::entity> entities;
    if (const auto* entityStorage = world.registry().storage<entt::entity>()) {
        for (auto it = entityStorage->begin(); it != entityStorage->end(); ++it) {
            entities.push_back(*it);
        }
    }
    std::sort(entities.begin(), entities.end(), [](entt::entity a, entt::entity b) {
        return entt::to_integral(a) < entt::to_integral(b);
    });

    // 5. Hash each entity and its components
    const auto& compEntries = componentRegistry();
    for (entt::entity e : entities) {
        uint32_t id = static_cast<uint32_t>(entt::to_integral(e));
        hashPod(hash, id);

        for (const auto& compEntry : compEntries) {
            compEntry.hashFn(world, e, hash);
        }
    }

    return hash;
}

} // namespace engine
