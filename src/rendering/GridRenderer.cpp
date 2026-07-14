#include "GridRenderer.h"
#include <glad/glad.h>

namespace {
    constexpr float kQuadVertices[] = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
    };
}

GridRenderer::GridRenderer() {
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    m_shader = std::make_unique<Shader>("shaders/grid.vert", "shaders/grid.frag");
}

GridRenderer::~GridRenderer() {
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
}

void GridRenderer::Render(const Camera& camera, float aspectRatio) {

    if (!Visible) return;

    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 proj = camera.GetProjectionMatrix(aspectRatio);
    glm::mat4 invViewProj = glm::inverse(proj * view);

    m_shader->Bind();
    m_shader->SetMat4("uInvViewProj", invViewProj);
    m_shader->SetMat4("uView", view);
    m_shader->SetMat4("uProj", proj);
    m_shader->SetFloat("uNear", m_near);
    m_shader->SetFloat("uFar", m_far);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}