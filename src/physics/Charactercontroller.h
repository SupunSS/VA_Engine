#pragma once
#include "PhysicsWorld.h"
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <glm/glm.hpp>
#include <memory>

// Kinematic character controller — walk, jump, gravity, slope/step handling.
// Wraps JPH::CharacterVirtual so the rest of the engine never touches Jolt
// types directly. Deliberately separate from Transform/Scene: PhysicsWorld
// owns the capsule, the ECS side just reads GetPosition() each frame and
// writes it into the entity's Transform.
class CharacterController {
public:
    CharacterController(PhysicsWorld& world, const glm::vec3& startPosition);
    ~CharacterController();

    // wishDirection is XZ-plane, camera-relative, NOT normalized-required.
    // jumpPressed triggers a jump if currently grounded.
    void Update(float deltaTime, const glm::vec3& wishDirection, bool jumpPressed);

    glm::vec3 GetPosition() const;
    void SetPosition(const glm::vec3& position); // teleport + zero velocity, used to respawn on Play
    bool IsGrounded() const;

    float WalkSpeed = 4.0f;
    float SprintSpeed = 8.0f;
    float JumpSpeed = 5.0f;
    bool Sprinting = false;

private:
    PhysicsWorld& m_world;
    std::unique_ptr<JPH::CharacterVirtual> m_character;
    glm::vec3 m_velocity{0.0f};

    static constexpr float kCapsuleRadius = 0.35f;
    static constexpr float kCapsuleHalfHeight = 0.9f;
    static constexpr float kGravity = -20.0f;
};