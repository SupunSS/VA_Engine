#include "CityLayout.h"
#include "CityLayoutConfig.h"

namespace CityLayout {

namespace {
std::vector<glm::vec3> BuildInsetLoop(int chunkX, int chunkZ, float chunkSize, float inset,
                                       float walkHeight, int pointsPerEdge) {
    const float worldX0 = chunkX * chunkSize;
    const float worldZ0 = chunkZ * chunkSize;

    const float minX = worldX0 + inset;
    const float maxX = worldX0 + chunkSize - inset;
    const float minZ = worldZ0 + inset;
    const float maxZ = worldZ0 + chunkSize - inset;

    const glm::vec3 corners[4] = {
        glm::vec3(minX, walkHeight, minZ),
        glm::vec3(maxX, walkHeight, minZ),
        glm::vec3(maxX, walkHeight, maxZ),
        glm::vec3(minX, walkHeight, maxZ),
    };

    std::vector<glm::vec3> loop;
    loop.reserve(4 * pointsPerEdge);

    const int segmentsPerEdge = pointsPerEdge > 0 ? pointsPerEdge : 1;
    for (int edge = 0; edge < 4; ++edge) {
        const glm::vec3& start = corners[edge];
        const glm::vec3& end = corners[(edge + 1) % 4];
        for (int seg = 0; seg < segmentsPerEdge; ++seg) {
            const float t = static_cast<float>(seg) / static_cast<float>(segmentsPerEdge);
            loop.push_back(glm::mix(start, end, t));
        }
    }

    return loop;
}
}

std::vector<glm::vec3> GetSidewalkLoopWaypoints(int chunkX, int chunkZ, float chunkSize) {
    const auto& config = CityLayoutConfig::Get();
    // Matches SceneLoader's sidewalk placement: inset = roadWidth (chunk
    // edge to the outer edge of the sidewalk ring) + half the sidewalk's
    // own width, landing exactly on the sidewalk's centerline.
    const float inset = config.RoadWidth + config.SidewalkWidth * 0.5f;
    return BuildInsetLoop(chunkX, chunkZ, chunkSize, inset, config.PedestrianWalkHeight, config.WaypointsPerEdge);
}

std::vector<glm::vec3> GetRoadLoopWaypoints(int chunkX, int chunkZ, float chunkSize) {
    const auto& config = CityLayoutConfig::Get();
    const float inset = config.RoadWidth * 0.5f;
    constexpr float kRoadWaypointHeight = 0.1f; // just above the road surface
    return BuildInsetLoop(chunkX, chunkZ, chunkSize, inset, kRoadWaypointHeight, config.WaypointsPerEdge);
}

} // namespace CityLayout