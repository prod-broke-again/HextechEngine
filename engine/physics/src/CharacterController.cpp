#include "engine/physics/CharacterController.hpp"
#include "engine/core/Log.hpp"

#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

namespace engine {

namespace Layers {
static constexpr JPH::ObjectLayer NON_MOVING = 0;
static constexpr JPH::ObjectLayer MOVING = 1;
}

struct CharacterController::Listener : public JPH::CharacterContactListener {
    void OnContactAdded(const JPH::CharacterVirtual* inCharacter, const JPH::BodyID& inBodyID2,
                        const JPH::SubShapeID& inSubShapeID2, JPH::RVec3Arg inContactPosition,
                        JPH::Vec3Arg inContactNormal, JPH::CharacterContactSettings& ioSettings) override {
        ioSettings.mCanReceiveImpulses = true;
        ioSettings.mCanPushCharacter = true;
    }
};

CharacterController::CharacterController() = default;
CharacterController::~CharacterController() = default;

void CharacterController::init(JoltWorld& world, const glm::vec3& startPos) {
    m_listener = std::make_unique<Listener>();

    JPH::CharacterVirtualSettings settings;
    settings.mMass = 80.0f;
    settings.mMaxStrength = 150.0f;
    settings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
    settings.mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces;
    settings.mCharacterPadding = 0.02f;
    settings.mPenetrationRecoverySpeed = 1.0f;
    settings.mPredictiveContactDistance = 0.1f;
    settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -0.35f);

    JPH::Ref<JPH::Shape> capsule = new JPH::CapsuleShape(0.55f, 0.35f);
    settings.mShape = JPH::RotatedTranslatedShapeSettings(
        JPH::Vec3(0.0f, 0.9f, 0.0f),
        JPH::Quat::sIdentity(),
        capsule
    ).Create().Get();

    m_character = new JPH::CharacterVirtual(
        &settings,
        JPH::RVec3(startPos.x, startPos.y, startPos.z),
        JPH::Quat::sIdentity(),
        &world.physics()
    );

    m_character->SetListener(m_listener.get());
    log(LogLevel::Info, "CharacterController initialized");
}

void CharacterController::update(JoltWorld& world, float deltaTime, const glm::vec2& moveInput, float yaw, bool jump) {
    if (!m_character) return;
    if (deltaTime <= 0.0f) return;

    // 1. Calculate desired horizontal movement direction in world space
    const glm::vec3 forward = glm::normalize(glm::vec3(std::cos(yaw), 0.0f, std::sin(yaw)));
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 wishDir = forward * moveInput.y + right * moveInput.x;
    const float inputLen = glm::length(wishDir);
    if (inputLen > 0.001f) {
        wishDir /= inputLen;
    } else {
        wishDir = glm::vec3(0.0f);
    }

    // 2. Fetch current velocity from Jolt
    const JPH::Vec3 joltVel = m_character->GetLinearVelocity();
    glm::vec3 currentHorizVel{joltVel.GetX(), 0.0f, joltVel.GetZ()};
    float verticalVel = joltVel.GetY();

    const bool grounded = (m_character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround);

    // 3. Ground vs Air Movement Physics
    if (grounded) {
        // Ground Jump
        if (jump) {
            verticalVel = jumpSpeed;
        } else {
            verticalVel = 0.0f;
        }

        // On Ground: responsive acceleration and crisp friction
        if (inputLen > 0.001f) {
            const glm::vec3 targetHorizVel = wishDir * walkSpeed;
            const glm::vec3 deltaV = targetHorizVel - currentHorizVel;
            currentHorizVel += deltaV * std::min(1.0f, groundAcceleration * deltaTime);
        } else {
            // Apply ground friction (decelerate naturally to zero)
            const float speed = glm::length(currentHorizVel);
            if (speed > 0.001f) {
                const float drop = speed * groundFriction * deltaTime;
                const float newSpeed = std::max(0.0f, speed - drop);
                currentHorizVel = (currentHorizVel / speed) * newSpeed;
            } else {
                currentHorizVel = glm::vec3(0.0f);
            }
        }
    } else {
        // In Air: MOMENTUM CONSERVATION (Standard shooter physics - NOT a jetpack!)
        // Gravity integration
        verticalVel -= gravity * deltaTime;
        verticalVel = std::max(verticalVel, -45.0f); // Terminal fall velocity

        // Air Control (Classic Quake / Source / Apex air-strafe physics):
        // Does NOT overwrite horizontal momentum! It only gently guides trajectory if wishDir is pressed.
        if (inputLen > 0.001f) {
            const float wishSpeed = std::min(walkSpeed, 6.0f);
            const float currentSpeedInWishDir = glm::dot(currentHorizVel, wishDir);
            const float addSpeed = wishSpeed - currentSpeedInWishDir;
            if (addSpeed > 0.0f) {
                float accelSpeed = airAcceleration * wishSpeed * deltaTime;
                accelSpeed = std::min(accelSpeed, addSpeed);
                currentHorizVel += wishDir * accelSpeed;
            }
        }
        // If inputLen == 0 in air: currentHorizVel is UNTOUCHED! The player sails smoothly in their jump arc!
    }

    // 4. Update linear velocity before character step
    m_character->SetLinearVelocity(JPH::Vec3(currentHorizVel.x, verticalVel, currentHorizVel.z));

    // 5. Extended update (stairs stepping + smooth floor tracking)
    JPH::PhysicsSystem& physics = world.physics();
    JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
    updateSettings.mStickToFloorStepDown = JPH::Vec3(0.0f, -0.5f, 0.0f);
    updateSettings.mWalkStairsStepUp = JPH::Vec3(0.0f, 0.4f, 0.0f);

    m_character->ExtendedUpdate(
        deltaTime,
        -JPH::Vec3::sAxisY() * gravity,
        updateSettings,
        physics.GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
        physics.GetDefaultLayerFilter(Layers::MOVING),
        {},
        {},
        world.tempAllocator()
    );
}

glm::vec3 CharacterController::position() const {
    if (!m_character) return glm::vec3(0.0f);
    const JPH::RVec3 pos = m_character->GetPosition();
    return glm::vec3(static_cast<float>(pos.GetX()), static_cast<float>(pos.GetY()), static_cast<float>(pos.GetZ()));
}

glm::vec3 CharacterController::velocity() const {
    if (!m_character) return glm::vec3(0.0f);
    const JPH::Vec3 vel = m_character->GetLinearVelocity();
    return glm::vec3(vel.GetX(), vel.GetY(), vel.GetZ());
}

bool CharacterController::isGrounded() const {
    if (!m_character) return false;
    return m_character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
}

void CharacterController::setPosition(const glm::vec3& pos) {
    if (!m_character) return;
    m_character->SetPosition(JPH::RVec3(pos.x, pos.y, pos.z));
    m_character->SetLinearVelocity(JPH::Vec3::sZero());
}

} // namespace engine
