#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <entt/entt.hpp>
#include <memory>

// Every entity that exists in the world has this. Cheap by design —
// static props (streetlights, trash cans) never touch anything beyond this.
struct Transform {
    glm::vec3 Position{0.0f};
    glm::quat Rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 Scale{1.0f};

    // Hierarchy support — parent is optional (entt::null if none).
    // Static-case-cheap: entities with no parent skip all hierarchy math.
    entt::entity Parent = entt::null;

    glm::mat4 GetLocalMatrix() const;
};

class Model; // forward declare from rendering
struct MeshRenderer {
    std::shared_ptr<Model> ModelRef;
};