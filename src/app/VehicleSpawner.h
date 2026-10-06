#pragma once
#include "../scene/Scene.h"
#include <glm/glm.hpp>

class PhysicsWorld;

// Spawns the low-poly test vehicle (chassis + 4 wheels + engine audio) at `position`.
// Returns the chassis entity.
entt::entity SpawnTestVehicle(Scene& scene, PhysicsWorld& physicsWorld, const glm::vec3& position);