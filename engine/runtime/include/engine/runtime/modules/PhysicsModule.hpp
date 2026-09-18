#pragma once

#include "engine/runtime/IModule.hpp"
#include "engine/world/World.hpp"
#include "engine/physics/JoltWorld.hpp"
#include "engine/physics/CharacterController.hpp"
#include "engine/world/bridges/PhysicsBridge.hpp"

namespace engine {

class PhysicsModule : public IModule {
public:
    std::string_view name() const override { return "PhysicsModule"; }

    void onAttach(World& world) override {
        world.emplaceResource<JoltWorld>();
        world.emplaceResource<CharacterController>();
    }

    void onDetach(World& world) override {
        if (world.hasResource<JoltWorld>()) {
            destroyPhysicsBodies(world.registry(), world.resource<JoltWorld>());
            world.removeResource<CharacterController>();
            world.removeResource<JoltWorld>();
        }
    }

    void tick(World& world) override {
        if (world.hasResource<JoltWorld>()) {
            auto& jolt = world.resource<JoltWorld>();
            jolt.step(1.0f / 30.0f);
            syncTransformsFromPhysics(world.registry(), jolt);
        }
    }
};

} // namespace engine
