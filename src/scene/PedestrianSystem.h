#pragma once
#include "../scene/Scene.h"
#include <glm/glm.hpp>

// Advances every entity with PedestrianAI + AnimatorComponent one frame:
// moves it toward its current waypoint, advances to the next waypoint (with
// a brief pause) on arrival, faces the direction of travel, and switches
// between idle/walk animation state accordingly.
//
// Only pedestrians within simulationRadius of viewerPosition are actually
// simulated (movement + skeletal animation) each frame — with a large
// loaded-chunk radius, hundreds of pedestrians can exist at once, and
// fully animating ones the camera could never see is pure wasted CPU. This
// is the single biggest lever for pedestrian-related frame time; nothing
// about their visual behavior changes for anything the player can
// actually reach or see.
namespace PedestrianSystem {
void Update(Scene& scene, float deltaTime, const glm::vec3& viewerPosition, float simulationRadius);
}