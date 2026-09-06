#pragma once

#include "engine/physics/JoltWorld.hpp"

#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <memory>

namespace engine {

class CharacterController {
public:
    CharacterController();
    ~CharacterController();

    CharacterController(const CharacterController&) = delete;
    CharacterController& operator=(const CharacterController&) = delete;

    void init(JoltWorld& world, const glm::vec3& startPos);
    void update(JoltWorld& world, float deltaTime, const glm::vec2& moveInput, float yaw, bool jump);

    [[nodiscard]] glm::vec3 position() const;
    [[nodiscard]] glm::vec3 velocity() const;
    [[nodiscard]] bool isGrounded() const;

    void setPosition(const glm::vec3& pos);

    float walkSpeed = 6.0f;
    float jumpSpeed = 5.5f;
    float eyeHeight = 1.6f;

private:
    struct Listener;
    std::unique_ptr<Listener> m_listener;
    JPH::Ref<JPH::CharacterVirtual> m_character;
};

} // namespace engine
