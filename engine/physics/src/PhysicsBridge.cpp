#include "engine/physics/PhysicsBridge.hpp"

#include "engine/core/Log.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

namespace engine {

namespace {

JPH::EMotionType motionType(bool dynamic) {
    return dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static;
}

JPH::ObjectLayer objectLayer(bool dynamic) {
    return dynamic ? static_cast<JPH::ObjectLayer>(1) : static_cast<JPH::ObjectLayer>(0);
}

void createBody(JoltWorld& world, entt::registry& registry, entt::entity entity,
                const glm::vec3& halfExtents, bool dynamic, float mass) {
    if (!registry.all_of<TransformLocal, RigidBodyComponent>(entity)) {
        return;
    }

    const auto& transform = registry.get<TransformLocal>(entity);
    auto& body = registry.get<RigidBodyComponent>(entity);

    JPH::BoxShapeSettings shapeSettings(
        JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z), 0.05f);
    const JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
    if (shapeResult.HasError()) {
        log(LogLevel::Error, shapeResult.GetError().c_str());
        return;
    }

    JPH::BodyCreationSettings settings(
        shapeResult.Get(), JPH::RVec3(transform.translation.x, transform.translation.y,
                                      transform.translation.z),
        JPH::Quat(transform.rotation.w, transform.rotation.x, transform.rotation.y,
                  transform.rotation.z),
        motionType(dynamic), objectLayer(dynamic));

    if (dynamic) {
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = mass;
    }

    JPH::BodyInterface& iface = world.bodyInterface();
    const JPH::BodyID bodyId = iface.CreateAndAddBody(settings, JPH::EActivation::Activate);
    body.bodyIndex = bodyId.GetIndex();
    body.dynamic = dynamic;
}

} // namespace

void createStaticBox(JoltWorld& world, entt::registry& registry, entt::entity entity,
                     const glm::vec3& halfExtents) {
    createBody(world, registry, entity, halfExtents, false, 0.f);
}

void createDynamicBox(JoltWorld& world, entt::registry& registry, entt::entity entity,
                      const glm::vec3& halfExtents, float mass) {
    createBody(world, registry, entity, halfExtents, true, mass);
}

void syncTransformsFromPhysics(entt::registry& registry, JoltWorld& world) {
    auto view = registry.view<TransformLocal, RigidBodyComponent>();
    JPH::BodyInterface& iface = world.bodyInterface();

    for (const auto entity : view) {
        const auto& body = view.get<RigidBodyComponent>(entity);
        if (!body.dynamic || body.bodyIndex == UINT32_MAX) {
            continue;
        }

        const JPH::BodyID bodyId(body.bodyIndex);
        if (!iface.IsAdded(bodyId)) {
            continue;
        }

        auto& transform = view.get<TransformLocal>(entity);
        const JPH::RVec3 pos = iface.GetPosition(bodyId);
        const JPH::Quat rot = iface.GetRotation(bodyId);
        transform.translation = {static_cast<float>(pos.GetX()), static_cast<float>(pos.GetY()),
                               static_cast<float>(pos.GetZ())};
        transform.rotation = {rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ()};
    }
}

void destroyPhysicsBodies(entt::registry& registry, JoltWorld& world) {
    auto view = registry.view<RigidBodyComponent>();
    JPH::BodyInterface& iface = world.bodyInterface();
    for (const auto entity : view) {
        const auto& body = view.get<RigidBodyComponent>(entity);
        if (body.bodyIndex == UINT32_MAX) {
            continue;
        }
        const JPH::BodyID bodyId(body.bodyIndex);
        if (iface.IsAdded(bodyId)) {
            iface.RemoveBody(bodyId);
            iface.DestroyBody(bodyId);
        }
    }
}

} // namespace engine
