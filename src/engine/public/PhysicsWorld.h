#pragma once

#include "Types.h"
#include <vector>

// ============================================================================
// PHYSICS WORLD INTERFACE
// ============================================================================

/**
 * Physics simulation world
 */
class PhysicsWorld {
public:
    virtual ~PhysicsWorld() = default;

    /**
     * Cast a ray and return all hits
     * @param origin Start position
     * @param direction Direction (should be normalized)
     * @param maxDistance Maximum distance
     * @return Vector of hits (closest first)
     */
    virtual std::vector<RaycastHit> RaycastAll(const Vec3& origin, const Vec3& direction, 
                                               float maxDistance = 1000.0f) = 0;

    /**
     * Cast a ray and return the closest hit
     * @param origin Start position
     * @param direction Direction (should be normalized)
     * @param maxDistance Maximum distance
     * @return Closest hit, or hit.hit == false if none
     */
    virtual RaycastHit Raycast(const Vec3& origin, const Vec3& direction, 
                              float maxDistance = 1000.0f) = 0;

    /**
     * Get all entities in a sphere
     * @param center Sphere center
     * @param radius Sphere radius
     * @return Vector of EntityIds within sphere
     */
    virtual std::vector<EntityId> QuerySphere(const Vec3& center, float radius) = 0;

    /**
     * Get all entities in a box
     * @param min Minimum corner
     * @param max Maximum corner
     * @return Vector of EntityIds within box
     */
    virtual std::vector<EntityId> QueryBox(const Vec3& min, const Vec3& max) = 0;

    /**
     * Set gravity
     * @param gravity Acceleration vector (usually (0, -9.81, 0))
     */
    virtual void SetGravity(const Vec3& gravity) = 0;

    /**
     * Get current gravity
     * @return Gravity vector
     */
    virtual Vec3 GetGravity() const = 0;

    /**
     * Simulate one physics step
     * @param deltaTime Time step in seconds
     */
    virtual void Update(float deltaTime) = 0;
};
