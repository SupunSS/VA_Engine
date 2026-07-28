#include "VehicleCamera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

void VehicleCamera::SetTarget(const glm::vec3& vehiclePos, const glm::quat& vehicleRot, float speedKmh, float deltaTime) {
    m_target = vehiclePos + glm::vec3(0.0f, m_heightOffset, 0.0f);

    glm::vec3 vehicleForward = vehicleRot * glm::vec3(0.0f, 0.0f, 1.0f);
    vehicleForward.y = 0.0f;
    
    if (glm::length(vehicleForward) > 0.001f) {
        vehicleForward = glm::normalize(vehicleForward);
    } else {
        vehicleForward = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    if (m_firstUpdate) {
        m_smoothForward = vehicleForward;
        m_firstUpdate = false;
    } else {
        // Prevent NaNs: If vectors are exactly opposite, mixing them results in a zero vector.
        // Normalizing a zero vector creates NaNs. We slightly perturb it to force a valid rotation direction.
        if (glm::dot(m_smoothForward, vehicleForward) < -0.999f) {
            m_smoothForward += glm::vec3(0.01f, 0.0f, 0.01f);
            m_smoothForward = glm::normalize(m_smoothForward);
        }
        
        // Smoothly interpolate forward direction
        float followSpeed = 6.0f;
        m_smoothForward = glm::normalize(glm::mix(m_smoothForward, vehicleForward, glm::min(followSpeed * deltaTime, 1.0f)));
    }

    // Combine vehicle forward with manual yaw/pitch offsets
    float baseYaw = glm::degrees(std::atan2(m_smoothForward.z, m_smoothForward.x));
    float finalYaw = baseYaw + m_yawOffset;
    float finalPitch = m_pitchOffset;

    glm::vec3 lookDir;
    lookDir.x = std::cos(glm::radians(finalYaw)) * std::cos(glm::radians(finalPitch));
    lookDir.y = std::sin(glm::radians(finalPitch));
    lookDir.z = std::sin(glm::radians(finalYaw)) * std::cos(glm::radians(finalPitch));
    lookDir = glm::normalize(lookDir);

    // Speed-dependent camera distance
    float dynamicDistance = m_baseDistance + (speedKmh / 100.0f) * 1.5f;

    Position = m_target - lookDir * dynamicDistance;

    // Decay manual yaw offset back to zero over time when driving
    if (speedKmh > 5.0f && std::abs(m_yawOffset) > 0.01f) {
        m_yawOffset = glm::mix(m_yawOffset, 0.0f, glm::min(2.0f * deltaTime, 1.0f));
    }
}

void VehicleCamera::ProcessMouseMovement(float xOffset, float yOffset) {
    m_yawOffset += xOffset * m_mouseSensitivity;
    m_pitchOffset += yOffset * m_mouseSensitivity;
    m_pitchOffset = std::clamp(m_pitchOffset, kMinPitch, kMaxPitch);
}

void VehicleCamera::ProcessScroll(float yOffset) {
    m_baseDistance = std::clamp(m_baseDistance - yOffset, kMinDistance, kMaxDistance);
}

glm::mat4 VehicleCamera::GetViewMatrix() const {
    return glm::lookAt(Position, m_target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 VehicleCamera::GetProjectionMatrix(float aspectRatio) const {
    return glm::perspective(glm::radians(m_fov), aspectRatio, 0.1f, 300.0f);
}