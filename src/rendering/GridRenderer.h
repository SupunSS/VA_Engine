#pragma once
#include "Shader.h"
#include "Camera.h"
#include <memory>

// Renders an infinite editor-style reference grid on the y=0 plane.
// No physical geometry — grid lines are computed analytically per-pixel
// in the fragment shader from reconstructed world position.
class GridRenderer {
public:
    GridRenderer();
    ~GridRenderer();

    void Render(const Camera& camera, float aspectRatio);

    bool Visible = true;

private:
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
    std::unique_ptr<Shader> m_shader;

    // Must match whatever near/far Camera::GetProjectionMatrix() uses internally.
    // Camera.h doesn't expose these — update if Camera.cpp's glm::perspective
    // call uses different values.
    float m_near = 0.1f;
    float m_far = 100.0f;
};