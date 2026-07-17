#pragma once
#include <glm/glm.hpp>

// Third-person orbit camera used in Play mode. Kept as its own class rather
// than extended onto Camera — the free-fly Camera is an editor tool with a
// totally different control scheme (WASD moves the camera itself); this one
// orbits a fixed distance around a target position (the player) and never
// moves independently.
class FollowCamera {
public:
    FollowCamera() = default;

    // Call every frame with the player's current position (feet/base).
    void SetTarget(const glm::vec3& targetPosition);

    void ProcessMouseMovement(float xOffset, float yOffset);
    void ProcessScroll(float yOffset);

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;

    // Direction the player should move in world space for a given input
    // axis, i.e. "forward" relative to where the camera is currently looking
    // (flattened onto XZ) — this is what makes WASD feel camera-relative.
    glm::vec3 GetForwardXZ() const;
    glm::vec3 GetRightXZ() const;

    glm::vec3 Position; // computed camera position, read after Update

private:
    glm::vec3 m_target{0.0f};

    float m_yaw = -90.0f;
    float m_pitch = -15.0f;
    float m_distance = 6.0f;
    float m_heightOffset = 1.6f; // roughly chest/head height on the target

    float m_mouseSensitivity = 0.15f;
    float m_fov = 60.0f;

    static constexpr float kMinPitch = -60.0f;
    static constexpr float kMaxPitch = 40.0f;
    static constexpr float kMinDistance = 2.0f;
    static constexpr float kMaxDistance = 15.0f;
};