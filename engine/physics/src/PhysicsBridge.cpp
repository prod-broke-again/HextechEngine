#include "engine/physics/PhysicsBridge.hpp"

#include "engine/core/Log.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>

namespace engine {

namespace {

JPH::EMotionType motionType(bool dynamic) {
    return dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static;
}

JPH::ObjectLayer objectLayer(bool dynamic) {
    return dynamic ? static_cast<JPH::ObjectLayer>(1) : static_cast<JPH::ObjectLayer>(0);
}

void createBodyWithShape(JoltWorld& world, entt::registry& registry, entt::entity entity,
                         const JPH::ShapeSettings* shapeSettings, bool dynamic, float mass) {
    if (!registry.all_of<TransformLocal, RigidBodyComponent>(entity)) {
        return;
    }

    const auto& transform = registry.get<TransformLocal>(entity);
    auto& body = registry.get<RigidBodyComponent>(entity);

    const JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings->Create();
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
    if (bodyId.IsInvalid()) {
        log(LogLevel::Error, "PhysicsBridge: CreateAndAddBody failed");
        return;
    }
    body.bodyIndex = bodyId.GetIndexAndSequenceNumber();
    body.dynamic = dynamic;
}

} // namespace

void createStaticBox(JoltWorld& world, entt::registry& registry, entt::entity entity,
                     const glm::vec3& halfExtents) {
    JPH::BoxShapeSettings shapeSettings(
        JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z), 0.05f);
    createBodyWithShape(world, registry, entity, &shapeSettings, false, 0.f);
}

void createDynamicBox(JoltWorld& world, entt::registry& registry, entt::entity entity,
                      const glm::vec3& halfExtents, float mass) {
    JPH::BoxShapeSettings shapeSettings(
        JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z), 0.05f);
    createBodyWithShape(world, registry, entity, &shapeSettings, true, mass);
}

void createDynamicConvexHull(JoltWorld& world, entt::registry& registry, entt::entity entity,
                             const std::vector<glm::vec3>& vertices, float mass) {
    if (vertices.empty()) return;
    std::vector<JPH::Vec3> joltVertices;
    joltVertices.reserve(vertices.size());
    for (const auto& v : vertices) {
        joltVertices.push_back(JPH::Vec3(v.x, v.y, v.z));
    }
    JPH::ConvexHullShapeSettings shapeSettings(joltVertices.data(), static_cast<int>(joltVertices.size()));
    createBodyWithShape(world, registry, entity, &shapeSettings, true, mass);
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
