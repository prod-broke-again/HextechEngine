#pragma once

#include <chrono>
#include <cstdint>

namespace engine {

class Time {
public:
    void tick();
    void reset();

    [[nodiscard]] float delta() const { return m_delta; }
    [[nodiscard]] float unscaledDelta() const { return m_unscaledDelta; }
    [[nodiscard]] double elapsed() const { return m_elapsed; }
    [[nodiscard]] uint64_t frameIndex() const { return m_frameIndex; }

    [[nodiscard]] float fps() const { return m_fps; }
    [[nodiscard]] float smoothedFps() const { return m_smoothedFps; }

    void setTimeScale(float scale) { m_timeScale = scale; }
    [[nodiscard]] float timeScale() const { return m_timeScale; }

    void setFixedDelta(float seconds) { m_fixedDelta = seconds; }
    [[nodiscard]] float fixedDelta() const { return m_fixedDelta; }

    void setMaxDelta(float seconds) { m_maxDelta = seconds; }
    [[nodiscard]] float maxDelta() const { return m_maxDelta; }

    [[nodiscard]] int consumeFixedSteps();

private:
    using Clock = std::chrono::steady_clock;

    Clock::time_point m_start{};
    Clock::time_point m_last{};
    double m_elapsed = 0.0;
    float m_delta = 0.f;
    float m_unscaledDelta = 0.f;
    float m_timeScale = 1.f;
    float m_fixedDelta = 1.f / 60.f;
    float m_maxDelta = 0.25f;
    float m_accumulator = 0.f;
    float m_fps = 0.f;
    float m_smoothedFps = 0.f;
    uint64_t m_frameIndex = 0;
    bool m_firstTick = true;
};

} // namespace engine
