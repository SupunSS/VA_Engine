#pragma once
#include <glm/glm.hpp>

class Camera {
public:
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f));

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;

    void ProcessKeyboard(bool forward, bool backward, bool left, bool right, float deltaTime);
    void ProcessMouseMovement(float xOffset, float yOffset);

    glm::vec3 Position;

private:
    void UpdateVectors();

    glm::vec3 m_front;
    glm::vec3 m_up;
    glm::vec3 m_right;
    glm::vec3 m_worldUp;

    float m_yaw = -90.0f;
    float m_pitch = 0.0f;

    float m_moveSpeed = 3.0f;
    float m_mouseSensitivity = 0.1f;
    float m_fov = 60.0f;
};