#pragma once

#include <cstdint>

namespace engine {

class Engine;

class FixedTimestepLoop {
public:
    explicit FixedTimestepLoop(Engine& engine);

    void update(float deltaTime);

private:
    Engine& m_engine;
    float m_accumulator = 0.0f;
};

} // namespace engine

