#include "engine/core/Time.hpp"

#include <algorithm>

namespace engine {

void Time::reset() {
    m_start = Clock::now();
    m_last = m_start;
    m_elapsed = 0.0;
    m_delta = 0.f;
    m_unscaledDelta = 0.f;
    m_accumulator = 0.f;
    m_fps = 0.f;
    m_smoothedFps = 0.f;
    m_frameIndex = 0;
    m_firstTick = true;
}

void Time::tick() {
    const Clock::time_point now = Clock::now();
    if (m_firstTick) {
        m_start = now;
        m_last = now;
        m_firstTick = false;
        return;
    }

    m_unscaledDelta = std::chrono::duration<float>(now - m_last).count();
    m_last = now;
    m_unscaledDelta = std::min(m_unscaledDelta, m_maxDelta);

    m_delta = m_unscaledDelta * m_timeScale;
    m_elapsed += static_cast<double>(m_unscaledDelta);
    ++m_frameIndex;

    if (m_unscaledDelta > 0.f) {
        m_fps = 1.f / m_unscaledDelta;
        if (m_smoothedFps <= 0.f) {
            m_smoothedFps = m_fps;
        } else {
            m_smoothedFps += (m_fps - m_smoothedFps) * 0.1f;
        }
    }

    m_accumulator += m_delta;
}

int Time::consumeFixedSteps() {
    int steps = 0;
    while (m_accumulator >= m_fixedDelta) {
        m_accumulator -= m_fixedDelta;
        ++steps;
    }
    return steps;
}

} // namespace engine
