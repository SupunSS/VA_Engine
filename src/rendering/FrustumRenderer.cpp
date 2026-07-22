#include "FrustumRenderer.h"
#include <glad/glad.h>
#include <array>

namespace {
// 12 edges of a cube, indexing into the 8 corners in the order they're
// unprojected below (0-3 = near-plane corners, 4-7 = far-plane corners,
// each face's 4 corners going bottom-left -> bottom-right -> top-right ->
// top-left so consecutive indices trace the face rectangle).
constexpr unsigned int kEdgeIndices[24] = {
    0, 1, 1, 2, 2, 3, 3, 0, // near face
    4, 5, 5, 6, 6, 7, 7, 4, // far face
    0, 4, 1, 5, 2, 6, 3, 7  // connecting edges
};
}

FrustumRenderer::FrustumRenderer() {
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    // 8 corners, filled in per-call in Render() — allocate now, upload later.
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * 8, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kEdgeIndices), kEdgeIndices, GL_STATIC_DRAW);

    glBindVertexArray(0);

    m_shader = std::make_unique<Shader>("shaders/frustum.vert", "shaders/frustum.frag");
}

FrustumRenderer::~FrustumRenderer() {
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
    glDeleteBuffers(1, &m_ebo);
}

void FrustumRenderer::Render(const glm::mat4& frustumViewProjection, const glm::mat4& viewProjection) {
    const glm::mat4 inv = glm::inverse(frustumViewProjection);

    auto Unproject = [&](float x, float y, float z) {
        glm::vec4 p = inv * glm::vec4(x, y, z, 1.0f);
        return glm::vec3(p) / p.w;
    };

    std::array<glm::vec3, 8> corners = {
        Unproject(-1, -1, -1), Unproject(1, -1, -1), Unproject(1, 1, -1), Unproject(-1, 1, -1), // near
        Unproject(-1, -1,  1), Unproject(1, -1,  1), Unproject(1, 1,  1), Unproject(-1, 1,  1), // far
    };

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec3) * corners.size(), corners.data());

    m_shader->Bind();
    m_shader->SetMat4("uViewProj", viewProjection);
    m_shader->SetVec3("uColor", glm::vec3(1.0f, 0.9f, 0.1f)); // bright yellow — reads clearly against most scenes

    glBindVertexArray(m_vao);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}