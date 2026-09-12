#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <functional>
#include <memory>
#include <glm/vec3.hpp>

namespace engine {

class JoltWorld {
public:
    JoltWorld();
    ~JoltWorld();

    JoltWorld(const JoltWorld&) = delete;
    JoltWorld& operator=(const JoltWorld&) = delete;

    void step(float deltaTime);

    [[nodiscard]] JPH::PhysicsSystem& physics();
    [[nodiscard]] JPH::BodyInterface& bodyInterface();
    [[nodiscard]] JPH::TempAllocator& tempAllocator();

    using ContactCallback = std::function<void(const glm::vec3& position, float impactSpeed)>;
    void setContactCallback(ContactCallback callback);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace engine
