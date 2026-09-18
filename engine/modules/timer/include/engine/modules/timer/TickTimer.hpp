#pragma once

#include <algorithm>
#include <cstdint>

namespace engine::timer {

class TickTimer {
public:
    constexpr TickTimer() = default;
    constexpr explicit TickTimer(uint32_t periodTicks, bool repeat = false)
        : m_periodTicks(periodTicks), m_repeat(repeat) {}

    bool tick(uint32_t deltaTicks = 1);

    void reset();
    void stop() { m_active = false; }
    void start() { m_active = true; }

    [[nodiscard]] bool isActive() const { return m_active; }
    [[nodiscard]] bool isRepeat() const { return m_repeat; }
    [[nodiscard]] bool isExpired() const { return m_elapsedTicks >= m_periodTicks; }

    [[nodiscard]] uint32_t period() const { return m_periodTicks; }
    void setPeriod(uint32_t periodTicks) { m_periodTicks = periodTicks; }

    [[nodiscard]] uint32_t elapsed() const { return m_elapsedTicks; }
    void setElapsed(uint32_t elapsedTicks) { m_elapsedTicks = elapsedTicks; }

    [[nodiscard]] float progress() const {
        if (m_periodTicks == 0) return 1.0f;
        return std::min(1.0f, static_cast<float>(m_elapsedTicks) / static_cast<float>(m_periodTicks));
    }

    constexpr bool operator==(const TickTimer& other) const {
        return m_periodTicks == other.m_periodTicks &&
               m_elapsedTicks == other.m_elapsedTicks &&
               m_repeat == other.m_repeat &&
               m_active == other.m_active;
    }

private:
    uint32_t m_periodTicks = 0;
    uint32_t m_elapsedTicks = 0;
    bool m_repeat = false;
    bool m_active = true;
};

struct TimerComponent {
    TickTimer timer;
};

} // namespace engine::timer
