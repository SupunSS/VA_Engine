#pragma once
#include "Shader.h"
#include <glm/glm.hpp>
#include <memory>

// Debug-only wireframe visualization of a camera frustum — draws the 12
// edges connecting the 8 corners obtained by unprojecting the NDC cube
// through a given frustum's inverse view-projection matrix. Backs the
// editor's "Freeze Culling Frustum" feature: freeze a snapshot of the
// active camera's frustum, then fly the free-fly camera anywhere and watch
// this wireframe box show exactly what Frustum::IntersectsSphere is being
// tested against.
class FrustumRenderer {
public:
    FrustumRenderer();
    ~FrustumRenderer();

    FrustumRenderer(const FrustumRenderer&) = delete;
    FrustumRenderer& operator=(const FrustumRenderer&) = delete;

    // frustumViewProjection: the frozen/target camera's view*projection —
    // the frustum being visualized, NOT necessarily the one currently
    // rendering this debug view.
    // viewProjection: the CURRENTLY rendering camera's own view*projection
    // — used to place these wireframe lines correctly in whatever view
    // you're actually looking through right now (so you can fly the
    // free-fly camera around a frozen frustum belonging to a different
    // camera pose entirely).
    void Render(const glm::mat4& frustumViewProjection, const glm::mat4& viewProjection);

private:
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
    unsigned int m_ebo = 0;
    std::unique_ptr<Shader> m_shader;
};