#include "VehicleController.h"
#include "../core/Log.h"
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <algorithm>
#include <cmath>

using JPH::Vec3;
using JPH::RVec3;
using JPH::Quat;
using JPH::Ref;
using JPH::RefConst;
using JPH::Shape;
using JPH::Body;
using JPH::BodyInterface;
using JPH::BodyCreationSettings;
using JPH::EMotionType;
using JPH::EActivation;
using JPH::EOverrideMassProperties;
using JPH::BoxShape;
using JPH::OffsetCenterOfMassShapeSettings;
using JPH::VehicleConstraint;
using JPH::VehicleConstraintSettings;
using JPH::VehicleCollisionTesterRay;
using JPH::VehicleDifferentialSettings;
using JPH::VehicleAntiRollBar;
using JPH::WheelSettings;
using JPH::WheelSettingsWV;
using JPH::Wheel;
using JPH::WheeledVehicleController;
using JPH::WheeledVehicleControllerSettings;
using JPH::ETransmissionMode;
using JPH::DegreesToRadians;

VehicleController::VehicleController(PhysicsWorld& world, const glm::vec3& position)
    : m_world(world) {
    CreateVehicle(position);
}

VehicleController::~VehicleController() {
    if (m_vehicleConstraint != nullptr) {
        m_world.GetSystem().RemoveConstraint(m_vehicleConstraint);
        m_world.GetSystem().RemoveStepListener(m_vehicleConstraint);
        m_vehicleConstraint = nullptr;
    }

    if (!m_chassisBodyId.IsInvalid()) {
        m_world.DestroyBody(m_chassisBodyId);
    }    
}

void VehicleController::CreateVehicle(const glm::vec3& position) {
    // 1. Chassis shape: Box half extents = (0.9m width, 0.4m height, 1.8f length)
    // Offset center of mass slightly lower (-0.2m) to increase anti-roll stability
    // Spawn height: wheels need their centers at ~0.35 (wheel radius) when resting.
    // suspension: attach at -0.1 from chassis, and with max length 0.45:
    // wheel_center = chassis_y - 0.1 - 0.45 = chassis_y - 0.55
    // For wheel_center = 0.35: chassis_y = 0.90
    constexpr float kSpawnHeightOffset = 0.90f;
    constexpr float kBodyCenterMassOffsetY = -0.2f;

    Vec3 halfExtents(0.9f, 0.4f, 1.8f);
    RefConst<Shape> boxShape = new BoxShape(halfExtents);
    RefConst<Shape> chassisShape = OffsetCenterOfMassShapeSettings(Vec3(0.0f, kBodyCenterMassOffsetY, 0.0f), boxShape).Create().Get();

    // 2. Chassis rigid body creation
    BodyCreationSettings chassisSettings(
        chassisShape,
        RVec3(position.x, position.y + kSpawnHeightOffset, position.z),
        Quat::sIdentity(),
        EMotionType::Dynamic,
        m_world.GetMovingLayer()
    );
    chassisSettings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
    chassisSettings.mMassPropertiesOverride.mMass = 1500.0f; // 1500 kg car

    BodyInterface& bodyInterface = m_world.GetBodyInterface();
    m_chassisBodyId = bodyInterface.CreateAndAddBody(chassisSettings, EActivation::Activate);

    Body* chassisBody = m_world.GetSystem().GetBodyLockInterface().TryGetBody(m_chassisBodyId);
    if (!chassisBody) {
        Log::Error("Failed to lock vehicle chassis body!");
        return;
    }

    // 3. Vehicle Constraint Settings
    VehicleConstraintSettings vehicleSettings;
    vehicleSettings.mUp = Vec3(0, 1, 0);
    vehicleSettings.mForward = Vec3(0, 0, 1);
    vehicleSettings.mMaxPitchRollAngle = DegreesToRadians(60.0f); // Prevents vehicle flipping completely upside down

    // 4. Wheels setup (0 = Front Left, 1 = Front Right, 2 = Rear Left, 3 = Rear Right)
    const float wheelRadius = 0.35f;
    const float wheelWidth = 0.25f;
    const float halfWidth = 0.85f;
    const float halfLength = 1.2f;
    const float suspensionOffset = -0.1f;

    Vec3 wheelPositions[4] = {
        Vec3(-halfWidth, suspensionOffset,  halfLength), // FL
        Vec3( halfWidth, suspensionOffset,  halfLength), // FR
        Vec3(-halfWidth, suspensionOffset, -halfLength), // RL
        Vec3( halfWidth, suspensionOffset, -halfLength)  // RR
    };

    for (int i = 0; i < 4; ++i) {
        Ref<WheelSettingsWV> wheel = new WheelSettingsWV();
        wheel->mPosition = wheelPositions[i];
        wheel->mRadius = wheelRadius;
        wheel->mWidth = wheelWidth;
        wheel->mSuspensionMinLength = 0.15f;
        wheel->mSuspensionMaxLength = 0.45f;
        wheel->mSuspensionSpring.mFrequency = m_suspensionFrequency;
        wheel->mSuspensionSpring.mDamping = m_suspensionDamping;
        wheel->mMaxSteerAngle = (i < 2) ? DegreesToRadians(m_maxSteerAngleDegrees) : 0.0f;
        wheel->mMaxBrakeTorque = (i < 2) ? 1800.0f : 1200.0f;
        wheel->mMaxHandBrakeTorque = (i >= 2) ? 4000.0f : 0.0f;

        // Middle-ground friction curves
        wheel->mLongitudinalFriction.Clear();
        wheel->mLongitudinalFriction.AddPoint(0.0f, m_tireFriction);
        wheel->mLongitudinalFriction.AddPoint(1.0f, m_tireFriction * 0.85f);

        wheel->mLateralFriction.Clear();
        wheel->mLateralFriction.AddPoint(0.0f, m_tireFriction);
        wheel->mLateralFriction.AddPoint(10.0f, m_tireFriction * 0.9f);
        wheel->mLateralFriction.AddPoint(25.0f, m_tireFriction * 0.7f);

        vehicleSettings.mWheels.push_back(Ref<WheelSettings>(wheel));
    }

    // 5. Wheeled Vehicle Controller (Engine, Transmission, Differentials)
    Ref<WheeledVehicleControllerSettings> controllerSettings = new WheeledVehicleControllerSettings();
    controllerSettings->mEngine.mMaxTorque = m_maxEngineTorque;
    controllerSettings->mEngine.mMinRPM = 1000.0f;
    controllerSettings->mEngine.mMaxRPM = 6000.0f;

    controllerSettings->mTransmission.mMode = ETransmissionMode::Auto;
    controllerSettings->mTransmission.mGearRatios = { 2.66f, 1.78f, 1.30f, 1.0f, 0.74f };
    controllerSettings->mTransmission.mReverseGearRatios = { -2.90f };
    controllerSettings->mTransmission.mShiftUpRPM = 4500.0f;
    controllerSettings->mTransmission.mShiftDownRPM = 2000.0f;

    // Differentials (RWD + FWD hybrid / 4WD for stability)
    VehicleDifferentialSettings diff;
    diff.mLeftWheel = 0;
    diff.mRightWheel = 1;
    diff.mDifferentialRatio = 3.42f;
    controllerSettings->mDifferentials.push_back(diff);

    VehicleDifferentialSettings diffRear;
    diffRear.mLeftWheel = 2;
    diffRear.mRightWheel = 3;
    diffRear.mDifferentialRatio = 3.42f;
    controllerSettings->mDifferentials.push_back(diffRear);

    vehicleSettings.mController = controllerSettings;

    // Anti-roll bars to prevent aggressive body roll from flipping the vehicle
    VehicleAntiRollBar frontAntiRoll;
    frontAntiRoll.mLeftWheel = 0;
    frontAntiRoll.mRightWheel = 1;
    frontAntiRoll.mStiffness = 1500.0f;
    vehicleSettings.mAntiRollBars.push_back(frontAntiRoll);

    VehicleAntiRollBar rearAntiRoll;
    rearAntiRoll.mLeftWheel = 2;
    rearAntiRoll.mRightWheel = 3;
    rearAntiRoll.mStiffness = 1500.0f;
    vehicleSettings.mAntiRollBars.push_back(rearAntiRoll);

    // 6. Create VehicleConstraint
    m_vehicleConstraint = new VehicleConstraint(*chassisBody, vehicleSettings);
    m_collisionTester = new VehicleCollisionTesterRay(m_world.GetMovingLayer());
    m_vehicleConstraint->SetVehicleCollisionTester(m_collisionTester);

    m_world.GetSystem().AddConstraint(m_vehicleConstraint);
    m_world.GetSystem().AddStepListener(m_vehicleConstraint);

    Log::Info("VehicleController created successfully at ({}, {}, {})", position.x, position.y, position.z);
}

void VehicleController::Update(float deltaTime, float throttle, float brake, float steerInput, bool handbrake) {
    if (m_vehicleConstraint == nullptr) {
        return;
    }

    // Jolt puts resting dynamic bodies to sleep; once asleep, driver input
    // has no effect until something explicitly wakes the body back up.
    // Without this, the vehicle can sit forever ignoring all input.
    m_world.GetBodyInterface().ActivateBody(m_chassisBodyId);

    WheeledVehicleController* controller = static_cast<WheeledVehicleController*>(m_vehicleConstraint->GetController());
    if (!controller) {
        return;
    }

    // Speed-sensitive steering reduction (reduces max steer angle at high speed to prevent twitchiness)
    float speedMs = m_world.GetBodyInterface().GetLinearVelocity(m_chassisBodyId).Length();
    float speedKmh = speedMs * 3.6f;
    float steerFactor = std::clamp(1.0f - (speedKmh / 140.0f) * 0.5f, 0.4f, 1.0f);

    float forwardInput = throttle; // -1 to 1 in auto transmission mode
    float rightInput = steerInput * steerFactor;
    float brakeInput = brake;
    float handbrakeInput = handbrake ? 1.0f : 0.0f;

    controller->SetDriverInput(forwardInput, rightInput, brakeInput, handbrakeInput);
}

void VehicleController::GetChassisTransform(glm::vec3& outPosition, glm::quat& outRotation) const {
    outPosition = m_world.GetBodyPosition(m_chassisBodyId);
    outRotation = m_world.GetBodyRotation(m_chassisBodyId);
}

void VehicleController::GetWheelTransform(int wheelIndex, glm::vec3& outPosition, glm::quat& outRotation) const {
    if (!m_vehicleConstraint || wheelIndex < 0 || wheelIndex >= 4) {
        outPosition = glm::vec3(0.0f);
        outRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    glm::vec3 chassisPos;
    glm::quat chassisRot;
    GetChassisTransform(chassisPos, chassisRot);

    const Wheel* wheel = m_vehicleConstraint->GetWheel(wheelIndex);
    const WheelSettings* settings = wheel->GetSettings();

    // Wheel local center = attachment position + suspension direction * suspension length
    Vec3 localPosVec = settings->mPosition + settings->mSuspensionDirection * wheel->GetSuspensionLength();
    glm::vec3 localPos(localPosVec.GetX(), localPosVec.GetY(), localPosVec.GetZ());

    outPosition = chassisPos + chassisRot * localPos;

    // Wheel rotation = chassisRot * steerRotation (around Y) * spinRotation (around X)
    glm::quat steerRot = glm::angleAxis(wheel->GetSteerAngle(), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat spinRot = glm::angleAxis(wheel->GetRotationAngle(), glm::vec3(1.0f, 0.0f, 0.0f));

    outRotation = chassisRot * steerRot * spinRot;
}

float VehicleController::GetSpeedKmh() const {
    if (m_chassisBodyId.IsInvalid()) return 0.0f;
    return m_world.GetBodyInterface().GetLinearVelocity(m_chassisBodyId).Length() * 3.6f;
}

float VehicleController::GetRPM() const {
    if (!m_vehicleConstraint) return 0.0f;
    const WheeledVehicleController* controller = static_cast<const WheeledVehicleController*>(m_vehicleConstraint->GetController());
    if (!controller) return 0.0f;
    return controller->GetEngine().GetCurrentRPM();
}

int VehicleController::GetTransmissionGear() const {
    if (!m_vehicleConstraint) return 0;
    const WheeledVehicleController* controller = static_cast<const WheeledVehicleController*>(m_vehicleConstraint->GetController());
    if (!controller) return 0;
    return controller->GetTransmission().GetCurrentGear();
}

void VehicleController::SetEngineTorque(float torque) {
    m_maxEngineTorque = torque;
    if (m_vehicleConstraint) {
        WheeledVehicleController* controller = static_cast<WheeledVehicleController*>(m_vehicleConstraint->GetController());
        if (controller) {
            controller->GetEngine().mMaxTorque = torque;
        }
    }
}

void VehicleController::SetSuspensionParameters(float frequency, float damping) {
    m_suspensionFrequency = frequency;
    m_suspensionDamping = damping;
    if (m_vehicleConstraint) {
        for (int i = 0; i < 4; ++i) {
            Wheel* wheel = m_vehicleConstraint->GetWheel(i);
            // Update live spring settings if available
            wheel->GetSettings(); // settings are immutable after constraint init in Jolt, but stored for parameter display
        }
    }
}

void VehicleController::SetTireFriction(float friction) {
    m_tireFriction = friction;
}

void VehicleController::SetMaxSteerAngleDegrees(float angleDegrees) {
    m_maxSteerAngleDegrees = angleDegrees;
}

void VehicleController::SetChassisTransform(const glm::vec3& position, const glm::quat& rotation) {
    if (m_chassisBodyId.IsInvalid()) return;
    m_world.GetBodyInterface().SetPositionAndRotation(
        m_chassisBodyId,
        JPH::RVec3(position.x, position.y, position.z),
        JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w),
        JPH::EActivation::Activate);
}
