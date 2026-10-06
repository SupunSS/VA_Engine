#include "FollowCamera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

void FollowCamera::SetTarget(const glm::vec3& targetPosition) {
    m_target = targetPosition + glm::vec3(0.0f, m_heightOffset, 0.0f);

    glm::vec3 direction;
    direction.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    direction.y = sin(glm::radians(m_pitch));
    direction.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));

    // Camera sits *behind* the target along -direction, orbiting at m_distance.
    Position = m_target - glm::normalize(direction) * m_distance;
}

void FollowCamera::ProcessMouseMovement(float xOffset, float yOffset) {
    m_yaw += xOffset * m_mouseSensitivity;
    m_pitch += yOffset * m_mouseSensitivity;
    m_pitch = std::clamp(m_pitch, kMinPitch, kMaxPitch);
}

void FollowCamera::ProcessScroll(float yOffset) {
    m_distance = std::clamp(m_distance - yOffset, kMinDistance, kMaxDistance);
}

glm::mat4 FollowCamera::GetViewMatrix() const {
    return glm::lookAt(Position, m_target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 FollowCamera::GetProjectionMatrix(float aspectRatio) const {
    return glm::perspective(glm::radians(m_fov), aspectRatio, 0.1f, 300.0f);
}

glm::vec3 FollowCamera::GetForwardXZ() const {
    glm::vec3 forward(cos(glm::radians(m_yaw)), 0.0f, sin(glm::radians(m_yaw)));
    return glm::normalize(forward);
}

glm::vec3 FollowCamera::GetRightXZ() const {
    glm::vec3 forward = GetForwardXZ();
    return glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
}