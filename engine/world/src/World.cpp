#include "engine/world/World.hpp"

namespace engine {

World::World() {
    m_deferred = std::make_unique<DeferredOps>(m_registry);
    for (size_t i = 0; i < static_cast<size_t>(RngStream::_Count); ++i) {
        m_rngs.emplace_back(0); // Default seed 0, will be configured properly later
    }
}

World::~World() = default;

} // namespace engine

