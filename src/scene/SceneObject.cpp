#include "SceneObject.h"
#include <glm/gtc/matrix_transform.hpp>

glm::mat4 SceneObject::GetModelMatrix() const {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
    m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0, 0, 1));
    m = glm::scale(m, scale);
    return m;
}