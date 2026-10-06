#pragma once
#include "PhysicsWorld.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>

class VehicleController {
public:
    VehicleController(PhysicsWorld& world, const glm::vec3& position);
    ~VehicleController();

    // Call once per frame from main update loop.
    // throttle: 0 to 1 (accelerate) or -1 to 0 (reverse in auto mode)
    // brake: 0 to 1
    // steerInput: -1 (left) to 1 (right)
    // handbrake: true/false
    void Update(float deltaTime, float throttle, float brake, float steerInput, bool handbrake);

    void GetChassisTransform(glm::vec3& outPosition, glm::quat& outRotation) const;
    void SetChassisTransform(const glm::vec3& position, const glm::quat& rotation);
    void GetWheelTransform(int wheelIndex, glm::vec3& outPosition, glm::quat& outRotation) const;

    float GetSpeedKmh() const;
    float GetRPM() const;
    int GetTransmissionGear() const;

    // Live tuning controls
    void SetEngineTorque(float torque);
    void SetSuspensionParameters(float frequency, float damping);
    void SetTireFriction(float friction);
    void SetMaxSteerAngleDegrees(float angleDegrees);

    float GetEngineTorque() const { return m_maxEngineTorque; }
    float GetSuspensionFrequency() const { return m_suspensionFrequency; }
    float GetSuspensionDamping() const { return m_suspensionDamping; }
    float GetTireFriction() const { return m_tireFriction; }
    float GetMaxSteerAngleDegrees() const { return m_maxSteerAngleDegrees; }

    JPH::BodyID GetBodyID() const { return m_chassisBodyId; }

private:
    PhysicsWorld& m_world;
    JPH::BodyID m_chassisBodyId{};
    JPH::Ref<JPH::VehicleConstraint> m_vehicleConstraint;
    JPH::Ref<JPH::VehicleCollisionTesterRay> m_collisionTester;

    float m_maxEngineTorque = 600.0f;
    float m_suspensionFrequency = 2.2f;
    float m_suspensionDamping = 0.85f;
    float m_tireFriction = 1.8f;
    float m_maxSteerAngleDegrees = 35.0f;

    void CreateVehicle(const glm::vec3& position);
};
