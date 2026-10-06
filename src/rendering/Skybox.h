#pragma once
#include <memory>
#include <glm/glm.hpp>
#include "Shader.h"

// Procedural gradient sky, rendered as a fullscreen background pass before
// any scene geometry. No texture assets required — colors are computed per
// pixel in sky.frag from the reconstructed view direction.
//
// This is a simplified stand-in for the kind of atmospheric scattering
// RAGE/GTA-style engines use (horizon/zenith blend + sun glow, no physically
// based Rayleigh/Mie scattering). If a full day-night cycle is wanted later,
// swap sky.frag for a scattering model — the Skybox class/call site here
// won't need to change.
class Skybox {
public:
    Skybox();
    ~Skybox();

    Skybox(const Skybox&) = delete;
    Skybox& operator=(const Skybox&) = delete;

    // view/projection should be whichever camera is active this frame
    // (editor free-cam or Play-mode follow camera) — Skybox doesn't care
    // which, it just needs the matrices and the world-space camera position.
    // sunDirection is the direction FROM the scene TOWARD the sun (i.e. the
    // negation of a "light travels this way" directional light vector).
    void Render(const glm::mat4& view, const glm::mat4& projection,
                const glm::vec3& cameraPosition, const glm::vec3& sunDirection);

private:
    unsigned int m_vao = 0;
    std::unique_ptr<Shader> m_shader;
};