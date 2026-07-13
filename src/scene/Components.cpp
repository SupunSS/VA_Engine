#include "Components.h"
#include <glm/gtc/matrix_transform.hpp>

glm::mat4 Transform::GetLocalMatrix() const {
    glm::mat4 mat = glm::translate(glm::mat4(1.0f), Position);
    mat *= glm::mat4_cast(Rotation);
    mat = glm::scale(mat, Scale);
    return mat;
}