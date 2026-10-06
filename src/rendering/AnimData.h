#pragma once
#include <glm/glm.hpp>

// One entry per unique bone name across the whole model. `id` indexes into
// the final bone-matrix array uploaded to the shader; `offsetMatrix` is the
// bone's inverse bind-pose transform (converts a vertex from model space
// into that bone's local space before the animated transform is applied).
struct BoneInfo {
    int id;
    glm::mat4 offsetMatrix;
};