#include "engine/modules/timer/TickTimer.hpp"

namespace engine::timer {

bool TickTimer::tick(uint32_t deltaTicks) {
    if (!m_active || m_periodTicks == 0) {
        return false;
    }

    m_elapsedTicks += deltaTicks;
    if (m_elapsedTicks >= m_periodTicks) {
        if (m_repeat) {
            m_elapsedTicks %= m_periodTicks;
        } else {
            m_elapsedTicks = m_periodTicks;
            m_active = false;
        }
        return true;
    }

    return false;
}

void TickTimer::reset() {
    m_elapsedTicks = 0;
    m_active = true;
}

} // namespace engine::timer
