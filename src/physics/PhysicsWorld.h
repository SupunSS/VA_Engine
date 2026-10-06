#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>

class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    void Step(float deltaTime);
    void SetGravity(const glm::vec3& gravity);
    void SetEnabled(bool enabled);
    bool IsEnabled() const;
    void DestroyBody(JPH::BodyID bodyId);
    bool CastRay(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, float& outHitDistance) const;
    bool RaycastClosest(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, glm::vec3& outHitPoint) const;

    JPH::BodyID CreateBoxBody(const glm::vec3& position, const glm::vec3& halfExtents, bool isStatic);
    JPH::BodyID CreateSphereBody(const glm::vec3& position, float radius, bool isStatic);
    JPH::PhysicsSystem& GetSystem() { return *m_physicsSystem; }
    JPH::TempAllocator& GetTempAllocator() { return *m_tempAllocator; }
    JPH::ObjectLayer GetMovingLayer() const;

    glm::vec3 GetBodyPosition(JPH::BodyID bodyId) const;
    glm::quat GetBodyRotation(JPH::BodyID bodyId) const;

    JPH::BodyInterface& GetBodyInterface();

private:
    bool m_enabled = true;
    std::unique_ptr<JPH::TempAllocatorImpl> m_tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> m_jobSystem;
    std::unique_ptr<JPH::PhysicsSystem> m_physicsSystem;

    // Jolt requires these layer/filter interface objects to stay alive
    class BroadPhaseLayerInterfaceImpl* m_broadPhaseLayerInterface;
    class ObjectVsBroadPhaseLayerFilterImpl* m_objectVsBroadPhaseFilter;
    class ObjectLayerPairFilterImpl* m_objectVsObjectFilter;
};