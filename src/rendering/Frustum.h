#pragma once
#include <glm/glm.hpp>
#include <array>

// View frustum extracted from a combined view-projection matrix using the
// standard Gribb-Hartmann plane extraction method. Used both for actual
// frustum culling (skip drawing anything provably outside the camera's
// view) and to drive the editor's frustum-wireframe debug visual.
class Frustum {
public:
    Frustum() = default;
    explicit Frustum(const glm::mat4& viewProjection);

    // Cheap conservative test — treats the object as a bounding sphere.
    // Returns true if the sphere is at least partially inside the frustum
    // (i.e. NOT provably fully outside any single plane). False positives
    // near corners are possible (sphere vs. plane is conservative vs. a
    // true frustum-vs-box test) but false negatives are not — nothing that
    // should be visible ever gets incorrectly culled.
    bool IntersectsSphere(const glm::vec3& center, float radius) const;

private:
    // Plane stored as (normal, distance) in ax+by+cz+d=0 form, with the
    // normal pointing INTO the frustum's interior. Order (left, right,
    // bottom, top, near, far) doesn't matter functionally — kept consistent
    // just for readability while debugging.
    struct Plane {
        glm::vec3 normal{0.0f};
        float distance = 0.0f;

        float SignedDistanceTo(const glm::vec3& point) const {
            return glm::dot(normal, point) + distance;
        }
    };

    std::array<Plane, 6> m_planes;
};