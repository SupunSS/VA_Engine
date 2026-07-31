#pragma once
#include <glm/glm.hpp>

class Scene;

// Per-frame audio housekeeping: keeps every AudioSource's position synced
// with its entity's Transform, triggers footstep one-shots off movement,
// and drives vehicle engine pitch/volume from RPM. Mirrors the shape of
// PedestrianSystem/PedestrianSpawnSystem — a static Update() called once
// per frame from main.cpp, no persistent system object to own.
namespace AudioSystem {

// Call once per frame, before Update(), with wherever the "ears" currently
// are — the editor camera, follow camera, or vehicle camera depending on
// mode. Forward/up should be normalized.
void UpdateListener(const glm::vec3& position, const glm::vec3& forward,
                     const glm::vec3& up, const glm::vec3& velocity = glm::vec3(0.0f));

// deltaTime should be 0 while stopped/in the editor so footstep timers
// don't keep advancing when nothing is actually walking.
void Update(Scene& scene, float deltaTime);

} // namespace AudioSystem