#pragma once

#include "Types.h"
#include <vector>

namespace VAPublic {

// ============================================================================
// PHYSICS WORLD INTERFACE
// ============================================================================

class IPhysicsWorld {
public:
    virtual ~IPhysicsWorld() = default;

    virtual std::vector<RaycastHit> RaycastAll(const Vec3& origin, const Vec3& direction,
                                               float maxDistance = 1000.0f) = 0;

    virtual RaycastHit Raycast(const Vec3& origin, const Vec3& direction,
                              float maxDistance = 1000.0f) = 0;

    virtual std::vector<EntityId> QuerySphere(const Vec3& center, float radius) = 0;

    virtual std::vector<EntityId> QueryBox(const Vec3& min, const Vec3& max) = 0;

    virtual void SetGravity(const Vec3& gravity) = 0;

    virtual Vec3 GetGravity() const = 0;

    virtual void Update(float deltaTime) = 0;
};

} // namespace VAPublic