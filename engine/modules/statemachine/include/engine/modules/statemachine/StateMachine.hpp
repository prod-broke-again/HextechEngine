#pragma once

#include "engine/foundation/StringHash.hpp"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace engine::statemachine {

struct StateMachineComponent {
    StringHash current{};
    StringHash previous{};
    uint32_t ticksInState = 0;

    void transitionTo(StringHash next) noexcept {
        previous = current;
        current = next;
        ticksInState = 0;
    }

    void tick(uint32_t delta = 1) noexcept {
        ticksInState += delta;
    }

    constexpr bool operator==(const StateMachineComponent& other) const noexcept {
        return current == other.current &&
               previous == other.previous &&
               ticksInState == other.ticksInState;
    }
};

template <typename StateId = StringHash>
class StateTransitionGraph {
public:
    void allowTransition(StateId from, StateId to) {
        m_transitions[from].insert(to);
    }

    [[nodiscard]] bool canTransition(StateId from, StateId to) const {
        auto it = m_transitions.find(from);
        if (it == m_transitions.end()) {
            return false;
        }
        return it->second.contains(to);
    }

private:
    std::unordered_map<StateId, std::unordered_set<StateId>> m_transitions;
};

} // namespace engine::statemachine
