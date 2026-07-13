#include "Scene.h"
#include "Components.h"

entt::entity Scene::CreateEntity() {
    entt::entity entity = Registry.create();
    Registry.emplace<Transform>(entity);
    return entity;
}

void Scene::DestroyEntity(entt::entity entity) {
    Registry.destroy(entity);
}

glm::mat4 Scene::GetWorldMatrix(entt::entity entity) const {
    const Transform& t = Registry.get<Transform>(entity);
    glm::mat4 local = t.GetLocalMatrix();

    if (t.Parent != entt::null && Registry.valid(t.Parent)) {
        return GetWorldMatrix(t.Parent) * local;
    }
    return local;
}