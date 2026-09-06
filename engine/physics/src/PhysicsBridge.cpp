#include "engine/physics/PhysicsBridge.hpp"

#include "engine/core/Log.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

namespace engine {

namespace {

JPH::EMotionType motionType(bool dynamic) {
    return dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static;
}

JPH::ObjectLayer objectLayer(bool dynamic) {
    return dynamic ? static_cast<JPH::ObjectLayer>(1) : static_cast<JPH::ObjectLayer>(0);
}

void createBodyWithShape(JoltWorld& world, entt::registry& registry, entt::entity entity,
                         const JPH::ShapeSettings* shapeSettings, bool dynamic, float mass,
                         const glm::vec3& initialVelocity = {0.f, 0.f, 0.f}) {
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
        if (glm::length(initialVelocity) > 0.001f) {
            settings.mLinearVelocity = JPH::Vec3(initialVelocity.x, initialVelocity.y, initialVelocity.z);
        }
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

void createDynamicSphere(JoltWorld& world, entt::registry& registry, entt::entity entity,
                         float radius, float mass, const glm::vec3& initialVelocity) {
    JPH::SphereShapeSettings shapeSettings(radius);
    createBodyWithShape(world, registry, entity, &shapeSettings, true, mass, initialVelocity);
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

void destroyPhysicsBody(JoltWorld& world, entt::registry& registry, entt::entity entity) {
    if (auto* body = registry.try_get<RigidBodyComponent>(entity)) {
        if (body->bodyIndex != UINT32_MAX) {
            const JPH::BodyID bodyId(body->bodyIndex);
            JPH::BodyInterface& iface = world.bodyInterface();
            if (iface.IsAdded(bodyId)) {
                iface.RemoveBody(bodyId);
                iface.DestroyBody(bodyId);
            }
            body->bodyIndex = UINT32_MAX;
        }
    }
}

void clearDynamicBodies(entt::registry& registry, JoltWorld& world) {
    auto view = registry.view<RigidBodyComponent>();
    JPH::BodyInterface& iface = world.bodyInterface();
    std::vector<entt::entity> toDestroy;

    for (const auto entity : view) {
        const auto& body = view.get<RigidBodyComponent>(entity);
        if (!body.dynamic || body.bodyIndex == UINT32_MAX) {
            continue;
        }
        const JPH::BodyID bodyId(body.bodyIndex);
        if (iface.IsAdded(bodyId)) {
            iface.RemoveBody(bodyId);
            iface.DestroyBody(bodyId);
        }
        toDestroy.push_back(entity);
    }

    for (const auto entity : toDestroy) {
        registry.destroy(entity);
    }
}

bool raycast(JoltWorld& world, const entt::registry& registry,
             const glm::vec3& origin, const glm::vec3& direction,
             float maxDistance, RaycastHit& hit) {
    hit = RaycastHit{};
    const float len = glm::length(direction);
    if (len < 0.0001f) {
        return false;
    }
    const glm::vec3 dir = direction / len;
    const JPH::RRayCast ray{
        JPH::RVec3(origin.x, origin.y, origin.z),
        JPH::Vec3(dir.x * maxDistance, dir.y * maxDistance, dir.z * maxDistance)
    };

    JPH::RayCastResult result;
    if (!world.physics().GetNarrowPhaseQuery().CastRay(ray, result)) {
        return false;
    }

    hit.hasHit = true;
    hit.distance = result.mFraction * maxDistance;
    const JPH::RVec3 hitPos = ray.GetPointOnRay(result.mFraction);
    hit.position = {static_cast<float>(hitPos.GetX()), static_cast<float>(hitPos.GetY()),
                    static_cast<float>(hitPos.GetZ())};
    hit.bodyIndex = result.mBodyID.GetIndexAndSequenceNumber();

    JPH::BodyLockRead lock(world.physics().GetBodyLockInterface(), result.mBodyID);
    if (lock.Succeeded()) {
        const JPH::Body& body = lock.GetBody();
        const JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, hitPos);
        hit.normal = {normal.GetX(), normal.GetY(), normal.GetZ()};
    }

    auto view = registry.view<const RigidBodyComponent>();
    for (const auto entity : view) {
        if (view.get<const RigidBodyComponent>(entity).bodyIndex == hit.bodyIndex) {
            hit.entity = entity;
            break;
        }
    }

    return true;
}

void applyImpulse(JoltWorld& world, entt::registry& registry, entt::entity entity,
                  const glm::vec3& impulse, const glm::vec3& point) {
    if (!registry.valid(entity) || !registry.all_of<RigidBodyComponent>(entity)) {
        return;
    }
    const auto& bodyComp = registry.get<RigidBodyComponent>(entity);
    if (!bodyComp.dynamic || bodyComp.bodyIndex == UINT32_MAX) {
        return;
    }
    const JPH::BodyID bodyId(bodyComp.bodyIndex);
    JPH::BodyInterface& iface = world.bodyInterface();
    if (iface.IsAdded(bodyId)) {
        iface.ActivateBody(bodyId);
        iface.AddImpulse(bodyId, JPH::Vec3(impulse.x, impulse.y, impulse.z),
                         JPH::RVec3(point.x, point.y, point.z));
    }
}

} // namespace engine
