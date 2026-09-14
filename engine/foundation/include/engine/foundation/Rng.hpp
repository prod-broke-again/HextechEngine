#pragma once

#include <cstdint>

namespace engine {
    
enum class RngStream : uint8_t { 
    Simulation, 
    Ai, 
    Loot, 
    Vfx, 
    _Count 
};

// Deterministic Random Number Generator using PCG32 algorithm.
class Rng {
public:
    constexpr Rng() = default;
    
    constexpr explicit Rng(uint64_t seed) {
        seedState(seed);
    }

    constexpr void seedState(uint64_t seedValue) {
        m_state = 0U;
        m_inc = (seedValue << 1u) | 1u;
        next();
        m_state += seedValue;
        next();
    }

    // Generate next 32-bit random number
    constexpr uint32_t next() {
        uint64_t oldState = m_state;
        // Advance internal state
        m_state = oldState * 6364136223846793005ULL + m_inc;
        // Calculate output function (XSH RR), uses old state for max ILP
        uint32_t xorshifted = static_cast<uint32_t>(((oldState >> 18u) ^ oldState) >> 27u);
        uint32_t rot = static_cast<uint32_t>(oldState >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
    }

    // Random float in [0.0, 1.0)
    constexpr float nextFloat() {
        return static_cast<float>(next() >> 8u) * (1.0f / 16777216.0f);
    }

    // Random float in [min, max)
    constexpr float nextFloat(float min, float max) {
        return min + nextFloat() * (max - min);
    }

    // Random int in [min, max]
    constexpr int nextInt(int min, int max) {
        if (max <= min) return min;
        uint32_t range = static_cast<uint32_t>(max - min + 1);
        uint32_t r = next();
        return min + static_cast<int>(r % range);
    }

private:
    uint64_t m_state = 0x853c49e6748fea9bULL;
    uint64_t m_inc = 0xda3e39cb94b95bdbULL;
};

} // namespace engine
