#pragma once
#include <entt/entt.hpp>
#include <glm/glm.hpp>

class Scene {
public:
    entt::entity CreateEntity();
    void DestroyEntity(entt::entity entity);

    glm::mat4 GetWorldMatrix(entt::entity entity) const;

    entt::registry Registry;
};