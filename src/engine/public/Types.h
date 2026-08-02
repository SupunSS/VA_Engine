#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <functional>
#include <entt/fwd.hpp>   
#include <glm/gtc/quaternion.hpp>
namespace VAPublic {

// ============================================================================
// FUNDAMENTAL TYPES
// ============================================================================

using EntityId = uint32_t;
using ComponentType = uint32_t;
using EventType = uint32_t;

// ============================================================================
// MATH TYPES
// ============================================================================

using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using Mat4 = glm::mat4;
using Quat = glm::quat;

// ============================================================================
// TRANSFORM (public-facing DTO — distinct from the internal ECS ::Transform)
// ============================================================================

struct Transform {
    Vec3 position = {0.0f, 0.0f, 0.0f};
    Quat rotation = {1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale = {1.0f, 1.0f, 1.0f};
};

// ============================================================================
// RENDERING
// ============================================================================

enum class RenderMode {
    Opaque,
    Transparent,
    Additive
};

struct MeshRef {
    uint32_t meshId = 0;
    bool visible = true;
};

// ============================================================================
// PHYSICS
// ============================================================================

enum class BodyType {
    Static,
    Dynamic,
    Kinematic
};

struct RaycastHit {
    EntityId entityId = 0;
    float distance = 0.0f;
    Vec3 hitPoint = {0.0f, 0.0f, 0.0f};
    Vec3 normal = {0.0f, 1.0f, 0.0f};
    bool hit = false;
};

// ============================================================================
// AUDIO
// ============================================================================

struct AudioClip {
    uint32_t clipId = 0;
    float volume = 1.0f;
    bool loop = false;
};

// ============================================================================
// INPUT
// ============================================================================

enum class KeyCode {
    W, A, S, D,
    Space, LeftShift,
    E, F,
    Escape,
    Mouse0, Mouse1,
    Up, Down, Left, Right
};

struct InputState {
    bool held[(int)KeyCode::Right + 1] = {};
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    bool mousePressed = false;
};

// ============================================================================
// CALLBACKS
// ============================================================================

using EntityCallback = std::function<void(EntityId)>;
using EventCallback = std::function<void(const void*)>;

} // namespace VAPublic