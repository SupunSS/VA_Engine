#include "PhysicsWorld.h"
#include "../core/Log.h"
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <algorithm>
#include <cstdarg>
#include <thread>

using namespace JPH;

// Layers: 0 = static/non-moving, 1 = dynamic/moving
namespace Layers {
    static constexpr ObjectLayer NON_MOVING = 0;
    static constexpr ObjectLayer MOVING = 1;
    static constexpr uint NUM_LAYERS = 2;
}

namespace BroadPhaseLayers {
    static constexpr BroadPhaseLayer NON_MOVING(0);
    static constexpr BroadPhaseLayer MOVING(1);
    static constexpr uint NUM_LAYERS = 2;
}

class BroadPhaseLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
    BroadPhaseLayerInterfaceImpl() {
        m_objectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        m_objectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }
    uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override {
        return m_objectToBroadPhase[layer];
    }
private:
    BroadPhaseLayer m_objectToBroadPhase[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl final : public ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(ObjectLayer, BroadPhaseLayer) const override { return true; }
};

class ObjectLayerPairFilterImpl final : public ObjectLayerPairFilter {
public:
    bool ShouldCollide(ObjectLayer, ObjectLayer) const override { return true; }
};

static void TraceImpl(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    Log::Info("[Jolt] {}", buffer);
}

PhysicsWorld::PhysicsWorld() {
    JPH::RegisterDefaultAllocator();
    JPH::Trace = TraceImpl;
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    m_tempAllocator = std::make_unique<TempAllocatorImpl>(10 * 1024 * 1024);
    const int workerThreads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);
    m_jobSystem = std::make_unique<JobSystemThreadPool>(
        cMaxPhysicsJobs, cMaxPhysicsBarriers,
        workerThreads);

    m_broadPhaseLayerInterface = new BroadPhaseLayerInterfaceImpl();
    m_objectVsBroadPhaseFilter = new ObjectVsBroadPhaseLayerFilterImpl();
    m_objectVsObjectFilter = new ObjectLayerPairFilterImpl();

    m_physicsSystem = std::make_unique<PhysicsSystem>();
    m_physicsSystem->Init(
        1024,   // max bodies
        0,      // num body mutexes (0 = default)
        1024,   // max body pairs
        1024,   // max contact constraints
        *m_broadPhaseLayerInterface,
        *m_objectVsBroadPhaseFilter,
        *m_objectVsObjectFilter
    );

    m_physicsSystem->SetGravity(Vec3(0.0f, -9.81f, 0.0f));

    Log::Info("Jolt Physics initialized");
}

PhysicsWorld::~PhysicsWorld() {
    delete m_broadPhaseLayerInterface;
    delete m_objectVsBroadPhaseFilter;
    delete m_objectVsObjectFilter;
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

void PhysicsWorld::Step(float deltaTime) {
    if (!m_enabled) {
        return;
    }

    const int collisionSteps = 1;
    m_physicsSystem->Update(deltaTime, collisionSteps, m_tempAllocator.get(), m_jobSystem.get());
}

void PhysicsWorld::SetGravity(const glm::vec3& gravity) {
    m_physicsSystem->SetGravity(Vec3(gravity.x, gravity.y, gravity.z));
}

void PhysicsWorld::SetEnabled(bool enabled) {
    m_enabled = enabled;
}

bool PhysicsWorld::IsEnabled() const {
    return m_enabled;
}

void PhysicsWorld::DestroyBody(JPH::BodyID bodyId) {
    if (bodyId.IsInvalid()) {
        return;
    }

    GetBodyInterface().RemoveBody(bodyId);
    GetBodyInterface().DestroyBody(bodyId);
}

BodyInterface& PhysicsWorld::GetBodyInterface() {
    return m_physicsSystem->GetBodyInterface();
}

JPH::BodyID PhysicsWorld::CreateBoxBody(const glm::vec3& position, const glm::vec3& halfExtents, bool isStatic) {
    BoxShapeSettings shapeSettings(Vec3(halfExtents.x, halfExtents.y, halfExtents.z));
    ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
    ShapeRefC shape = shapeResult.Get();

    BodyCreationSettings bodySettings(
        shape,
        RVec3(position.x, position.y, position.z),
        Quat::sIdentity(),
        isStatic ? EMotionType::Static : EMotionType::Dynamic,
        isStatic ? Layers::NON_MOVING : Layers::MOVING
    );

    BodyID id = GetBodyInterface().CreateAndAddBody(bodySettings, EActivation::Activate);
    return id;
}

JPH::BodyID PhysicsWorld::CreateSphereBody(const glm::vec3& position, float radius, bool isStatic) {
    SphereShapeSettings shapeSettings(radius);
    ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
    ShapeRefC shape = shapeResult.Get();

    BodyCreationSettings bodySettings(
        shape,
        RVec3(position.x, position.y, position.z),
        Quat::sIdentity(),
        isStatic ? EMotionType::Static : EMotionType::Dynamic,
        isStatic ? Layers::NON_MOVING : Layers::MOVING
    );

    BodyID id = GetBodyInterface().CreateAndAddBody(bodySettings, EActivation::Activate);
    return id;
}

glm::vec3 PhysicsWorld::GetBodyPosition(JPH::BodyID bodyId) const {
    RVec3 pos = m_physicsSystem->GetBodyInterface().GetPosition(bodyId);
    return glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
}

glm::quat PhysicsWorld::GetBodyRotation(JPH::BodyID bodyId) const {
    Quat rot = m_physicsSystem->GetBodyInterface().GetRotation(bodyId);
    return glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());
}

JPH::ObjectLayer PhysicsWorld::GetMovingLayer() const {
    return Layers::MOVING;
}

bool PhysicsWorld::CastRay(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, float& outHitDistance) const {
    JPH::RRayCast ray{
        JPH::RVec3(origin.x, origin.y, origin.z),
        JPH::Vec3(direction.x, direction.y, direction.z) * maxDistance
    };

    JPH::RayCastResult hit;
    hit.Reset();
    bool hadHit = m_physicsSystem->GetNarrowPhaseQuery().CastRay(ray, hit);
    if (hadHit) {
        outHitDistance = hit.mFraction * maxDistance;
        return true;
    }
    return false;
}

bool PhysicsWorld::RaycastClosest(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, glm::vec3& outHitPoint) const {
    RRayCast ray(
        RVec3(origin.x, origin.y, origin.z),
        Vec3(direction.x, direction.y, direction.z) * maxDistance
    );

    RayCastResult result;
    const bool hit = m_physicsSystem->GetNarrowPhaseQuery().CastRay(ray, result);
    if (!hit) {
        return false;
    }

    RVec3 hitPos = ray.GetPointOnRay(result.mFraction);
    outHitPoint = glm::vec3(hitPos.GetX(), hitPos.GetY(), hitPos.GetZ());
    return true;
}