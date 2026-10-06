#pragma once
#include <glm/glm.hpp>

class Camera {
public:
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f));

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;

    void ProcessKeyboard(bool forward, bool backward, bool left, bool right, float deltaTime);
    void ProcessMouseMovement(float xOffset, float yOffset);

    // Speed control — public so both ImGui and the scroll callback can drive it.
    float GetMoveSpeed() const { return m_moveSpeed; }
    void SetMoveSpeed(float speed);
    void AdjustMoveSpeed(float delta); // relative change, used by ctrl+scroll

    // Needed so main.cpp can capture the camera's starting look direction
    // and restore it exactly when Play mode ends (Position alone isn't
    // enough — yaw/pitch determine which way the camera is actually facing).
    float GetYaw() const { return m_yaw; }
    float GetPitch() const { return m_pitch; }
    glm::vec3 GetFront() const { return m_front; }
    void SetYawPitch(float yaw, float pitch) {
        m_yaw = yaw;
        m_pitch = pitch;
        UpdateVectors();
    }

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

    // Clamp range for scroll/UI adjustment — tune to taste.
    static constexpr float kMinMoveSpeed = 0.5f;
    static constexpr float kMaxMoveSpeed = 50.0f;
};