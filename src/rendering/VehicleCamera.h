#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "../physics/PhysicsWorld.h"

class VehicleCamera {
public:
    VehicleCamera() = default;

    void SetTarget(const glm::vec3& vehiclePos, const glm::quat& vehicleRot, float speedKmh, float deltaTime);

    void ProcessMouseMovement(float xOffset, float yOffset);
    void ProcessScroll(float yOffset);

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;

    glm::vec3 Position{0.0f};

private:
    glm::vec3 m_target{0.0f};
    glm::vec3 m_smoothPosition{0.0f};
    glm::vec3 m_smoothForward{0.0f, 0.0f, 1.0f};

    float m_yawOffset = 0.0f;
    float m_pitchOffset = -10.0f;
    float m_baseDistance = 7.0f;
    float m_heightOffset = 2.0f;
    float m_fov = 65.0f;
    float m_mouseSensitivity = 0.15f;
    bool m_firstUpdate = true;

    static constexpr float kMinDistance = 3.0f;
    static constexpr float kMaxDistance = 15.0f;
    static constexpr float kMinPitch = -40.0f;
    static constexpr float kMaxPitch = 30.0f;
};
