#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <memory>

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

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace engine
