#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

Camera::Camera(glm::vec3 position)
    : Position(position), m_worldUp(0.0f, 1.0f, 0.0f) {
    UpdateVectors();
}

glm::mat4 Camera::GetViewMatrix() const {
    return glm::lookAt(Position, Position + m_front, m_up);
}

glm::mat4 Camera::GetProjectionMatrix(float aspectRatio) const {
    return glm::perspective(glm::radians(m_fov), aspectRatio, 0.1f, 100.0f);
}

void Camera::ProcessKeyboard(bool forward, bool backward, bool left, bool right, float deltaTime) {
    float velocity = m_moveSpeed * deltaTime;
    if (forward)  Position += m_front * velocity;
    if (backward) Position -= m_front * velocity;
    if (left)     Position -= m_right * velocity;
    if (right)    Position += m_right * velocity;
}

void Camera::ProcessMouseMovement(float xOffset, float yOffset) {
    m_yaw   += xOffset * m_mouseSensitivity;
    m_pitch += yOffset * m_mouseSensitivity;

    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);

    UpdateVectors();
}

void Camera::UpdateVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_front = glm::normalize(front);

    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up    = glm::normalize(glm::cross(m_right, m_front));
}

void Camera::SetMoveSpeed(float speed) {
    m_moveSpeed = std::clamp(speed, kMinMoveSpeed, kMaxMoveSpeed);
}

void Camera::AdjustMoveSpeed(float delta) {
    SetMoveSpeed(m_moveSpeed + delta);
}