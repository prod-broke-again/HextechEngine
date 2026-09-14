#include "engine/ecs/Components.hpp"
#include "engine/foundation/TypeRegistry.hpp"

namespace engine {

void registerEngineComponents(TypeRegistry& types) {
    types.registerComponent<TransformLocal>("TransformLocal", 1)
        .field("translation", &TransformLocal::translation)
        .field("rotation", &TransformLocal::rotation)
        .field("scale", &TransformLocal::scale);

    types.registerComponent<TagComponent>("TagComponent", 1)
        .field("tag", &TagComponent::tag);

    types.registerComponent<MeshGeometryComponent>("MeshGeometryComponent", 1)
        .field("type", &MeshGeometryComponent::type)
        .field("assetPath", &MeshGeometryComponent::assetPath)
        .field("params", &MeshGeometryComponent::params);

    types.registerComponent<MeshComponent>("MeshComponent", 1)
        .field("tint", &MeshComponent::tint)
        .field("baseColorFactor", &MeshComponent::baseColorFactor)
        .field("metallic", &MeshComponent::metallic)
        .field("roughness", &MeshComponent::roughness)
        .field("emissiveIntensity", &MeshComponent::emissiveIntensity)
        .transientField("mesh", &MeshComponent::mesh)
        .transientField("baseColorTexture", &MeshComponent::baseColorTexture);

    types.registerComponent<ColliderComponent>("ColliderComponent", 1)
        .field("shape", &ColliderComponent::shape)
        .field("halfExtents", &ColliderComponent::halfExtents)
        .field("radius", &ColliderComponent::radius)
        .field("isStatic", &ColliderComponent::isStatic)
        .field("mass", &ColliderComponent::mass);

    types.registerComponent<PointLightComponent>("PointLightComponent", 1)
        .field("color", &PointLightComponent::color)
        .field("intensity", &PointLightComponent::intensity)
        .field("radius", &PointLightComponent::radius);

    types.registerComponent<CameraComponent>("CameraComponent", 1)
        .field("fovDegrees", &CameraComponent::fovDegrees)
        .field("nearPlane", &CameraComponent::nearPlane)
        .field("farPlane", &CameraComponent::farPlane)
        .field("active", &CameraComponent::active);

    types.registerComponent<FreeFlyController>("FreeFlyController", 1)
        .field("yaw", &FreeFlyController::yaw)
        .field("pitch", &FreeFlyController::pitch)
        .field("moveSpeed", &FreeFlyController::moveSpeed)
        .field("lookSensitivity", &FreeFlyController::lookSensitivity);

    types.registerComponent<AudioSourceComponent>("AudioSourceComponent", 1)
        .field("soundPath", &AudioSourceComponent::soundPath)
        .field("volume", &AudioSourceComponent::volume)
        .field("minDistance", &AudioSourceComponent::minDistance)
        .field("maxDistance", &AudioSourceComponent::maxDistance)
        .field("loop", &AudioSourceComponent::loop)
        .field("playOnAwake", &AudioSourceComponent::playOnAwake);

    types.registerComponent<ParticleEmitterComponent>("ParticleEmitterComponent", 1)
        .field("active", &ParticleEmitterComponent::active)
        .field("spawnRate", &ParticleEmitterComponent::spawnRate)
        .field("timer", &ParticleEmitterComponent::timer)
        .field("startColor", &ParticleEmitterComponent::startColor)
        .field("endColor", &ParticleEmitterComponent::endColor)
        .field("startSize", &ParticleEmitterComponent::startSize)
        .field("endSize", &ParticleEmitterComponent::endSize)
        .field("lifetime", &ParticleEmitterComponent::lifetime)
        .field("initialVelocity", &ParticleEmitterComponent::initialVelocity)
        .field("velocitySpread", &ParticleEmitterComponent::velocitySpread);
}

} // namespace engine
