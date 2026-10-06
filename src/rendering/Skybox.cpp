#include "Skybox.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_inverse.hpp>

Skybox::Skybox() {
    // The fullscreen triangle needs no vertex buffer — sky.vert generates
    // positions from gl_VertexID — but core-profile OpenGL still requires a
    // VAO bound for glDrawArrays to be valid, so we keep an empty one around.
    glGenVertexArrays(1, &m_vao);
    m_shader = std::make_unique<Shader>("shaders/sky.vert", "shaders/sky.frag");
}

Skybox::~Skybox() {
    glDeleteVertexArrays(1, &m_vao);
}

void Skybox::Render(const glm::mat4& view, const glm::mat4& projection,
                     const glm::vec3& cameraPosition, const glm::vec3& sunDirection) {
    // Sky is drawn first, behind everything, so it never needs to occlude or
    // be occluded — depth test/write are switched off for this one draw call
    // and restored immediately after so scene geometry (drawn right after
    // this call in main.cpp) depth-tests normally.
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    m_shader->Bind();

    const glm::mat4 inverseViewProjection = glm::inverse(projection * view);
    m_shader->SetMat4("uInvViewProj", inverseViewProjection);
    m_shader->SetVec3("uCameraPos", cameraPosition);
    m_shader->SetVec3("uSunDirection", glm::normalize(sunDirection));

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}