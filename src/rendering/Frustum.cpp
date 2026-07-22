#include "Frustum.h"

namespace {
void NormalizePlane(glm::vec3& normal, float& distance) {
    float length = glm::length(normal);
    if (length > 0.00001f) {
        normal /= length;
        distance /= length;
    }
}
}

Frustum::Frustum(const glm::mat4& m) {
    // Gribb-Hartmann extraction, adapted for GLM's column-major m[col][row]
    // storage. m must map world space directly to clip space (m ==
    // projection * view), matching how gl_Position is built elsewhere in
    // this engine (uProjection * uView * worldPos in triangle.vert).
    m_planes[0].normal   = glm::vec3(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0]); // left
    m_planes[0].distance = m[3][3] + m[3][0];

    m_planes[1].normal   = glm::vec3(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0]); // right
    m_planes[1].distance = m[3][3] - m[3][0];

    m_planes[2].normal   = glm::vec3(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1]); // bottom
    m_planes[2].distance = m[3][3] + m[3][1];

    m_planes[3].normal   = glm::vec3(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1]); // top
    m_planes[3].distance = m[3][3] - m[3][1];

    m_planes[4].normal   = glm::vec3(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2]); // near
    m_planes[4].distance = m[3][3] + m[3][2];

    m_planes[5].normal   = glm::vec3(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2]); // far
    m_planes[5].distance = m[3][3] - m[3][2];

    for (auto& plane : m_planes) {
        NormalizePlane(plane.normal, plane.distance);
    }
}

bool Frustum::IntersectsSphere(const glm::vec3& center, float radius) const {
    for (const auto& plane : m_planes) {
        if (plane.SignedDistanceTo(center) < -radius) {
            return false; // fully outside this one plane => fully outside the frustum
        }
    }
    return true;
}