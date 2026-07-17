#include "CharacterController.h"
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

CharacterController::CharacterController(PhysicsWorld& world, const glm::vec3& startPosition)
    : m_world(world) {

    // Capsule centered on the origin, then offset up by half-height so the
    // character's *feet* sit at startPosition (matches how Transform.Position
    // is used everywhere else in the engine — feet/base, not center).
    JPH::RefConst<JPH::Shape> capsule = new JPH::CapsuleShape(kCapsuleHalfHeight, kCapsuleRadius);
    JPH::RefConst<JPH::Shape> offsetShape = new JPH::RotatedTranslatedShape(
        JPH::Vec3(0, kCapsuleHalfHeight + kCapsuleRadius, 0), JPH::Quat::sIdentity(), capsule);

    JPH::CharacterVirtualSettings settings;
    settings.mShape = offsetShape;
    settings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
    settings.mMass = 80.0f;

    m_character = std::make_unique<JPH::CharacterVirtual>(
        &settings,
        JPH::RVec3(startPosition.x, startPosition.y, startPosition.z),
        JPH::Quat::sIdentity(),
        /* userData */ 0,
        &m_world.GetSystem());
}

CharacterController::~CharacterController() = default;

void CharacterController::Update(float deltaTime, const glm::vec3& wishDirection, bool jumpPressed) {
    bool grounded = IsGrounded();

    float speed = Sprinting ? SprintSpeed : WalkSpeed;
    glm::vec3 horizontalVelocity(0.0f);
    if (glm::length(wishDirection) > 0.001f) {
        horizontalVelocity = glm::normalize(wishDirection) * speed;
    }

    m_velocity.x = horizontalVelocity.x;
    m_velocity.z = horizontalVelocity.z;

    if (grounded) {
        m_velocity.y = -1.0f; // small downward bias keeps the character stuck to slopes
        if (jumpPressed) {
            m_velocity.y = JumpSpeed;
        }
    } else {
        m_velocity.y += kGravity * deltaTime;
    }

    m_character->SetLinearVelocity(JPH::Vec3(m_velocity.x, m_velocity.y, m_velocity.z));

    JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;

    JPH::DefaultBroadPhaseLayerFilter broadPhaseLayerFilter =
        m_world.GetSystem().GetDefaultBroadPhaseLayerFilter(m_world.GetMovingLayer());
    JPH::DefaultObjectLayerFilter objectLayerFilter =
        m_world.GetSystem().GetDefaultLayerFilter(m_world.GetMovingLayer());
    JPH::BodyFilter bodyFilter;
    JPH::ShapeFilter shapeFilter;

    m_character->ExtendedUpdate(
        deltaTime,
        JPH::Vec3(0, kGravity, 0),
        updateSettings,
        broadPhaseLayerFilter,
        objectLayerFilter,
        bodyFilter,
        shapeFilter,
        m_world.GetTempAllocator());
}

glm::vec3 CharacterController::GetPosition() const {
    JPH::RVec3 pos = m_character->GetPosition();
    return glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
}

void CharacterController::SetPosition(const glm::vec3& position) {
    m_character->SetPosition(JPH::RVec3(position.x, position.y, position.z));
    m_character->SetLinearVelocity(JPH::Vec3::sZero());
    m_velocity = glm::vec3(0.0f); // clears accumulated fall speed from before the reset
}

bool CharacterController::IsGrounded() const {
    return m_character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
}