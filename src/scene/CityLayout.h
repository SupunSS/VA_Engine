#pragma once
#include <glm/glm.hpp>
#include <vector>

// Waypoint-loop generation for NPC pathing, matching the road/sidewalk
// geometry SceneLoader actually builds. Dimensions come from
// CityLayoutConfig at call time (not compile-time constants), so editing
// config/city_layout.json changes both the visual layout AND where NPCs
// walk/drive, with nothing able to drift out of sync between them.
namespace CityLayout {

// Closed loop of waypoints along the CENTER of the sidewalk ring for the
// chunk at (chunkX, chunkZ) — for pedestrians to wander along.
std::vector<glm::vec3> GetSidewalkLoopWaypoints(int chunkX, int chunkZ, float chunkSize);

// Same idea, along the center of the ROAD ring — for traffic (added next).
std::vector<glm::vec3> GetRoadLoopWaypoints(int chunkX, int chunkZ, float chunkSize);

} // namespace CityLayout