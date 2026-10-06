#pragma once
#include <memory>
#include <glm/glm.hpp>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include "rendering/Model.h"
#include "rendering/Material.h"

struct SceneObject {
    std::shared_ptr<Model> model;
    std::shared_ptr<Material> material;

    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);

    JPH::BodyID physicsBody = JPH::BodyID(JPH::BodyID::cInvalidBodyID);

    glm::mat4 GetModelMatrix() const;
};