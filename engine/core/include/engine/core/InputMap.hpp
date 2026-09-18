#pragma once

#include "engine/core/Input.hpp"
#include "engine/foundation/StringHash.hpp"

#include <glm/vec2.hpp>
#include <unordered_map>

namespace engine {

using ActionId = StringHash;

namespace Actions {
    inline constexpr ActionId MoveForward = "move_forward"_sh;
    inline constexpr ActionId MoveBack    = "move_back"_sh;
    inline constexpr ActionId MoveLeft    = "move_left"_sh;
    inline constexpr ActionId MoveRight   = "move_right"_sh;
    inline constexpr ActionId Sprint      = "sprint"_sh;
    inline constexpr ActionId Jump        = "jump"_sh;
    inline constexpr ActionId Look        = "look"_sh;
}

class InputMap {
public:
    void bind(ActionId action, int key);
    void bindMouse(ActionId action, int button);

    [[nodiscard]] bool actionDown(const Input& input, ActionId action) const;
    [[nodiscard]] bool actionPressed(const Input& input, ActionId action) const;
    [[nodiscard]] glm::vec2 lookDelta() const { return m_lookDelta; }

    void beginFrame(const Input& input);

private:
    std::unordered_map<ActionId, int> m_keys;
    std::unordered_map<ActionId, int> m_buttons;
    glm::vec2 m_lookDelta{0.f};
};

} // namespace engine
