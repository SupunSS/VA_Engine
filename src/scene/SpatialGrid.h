#pragma once
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

// Uniform grid over the XZ plane (ground plane), bucketed by cell.
// This is intentionally simple — it's the seed for Phase 5 streaming,
// not a final solution. Swappable later for a quadtree/octree if needed.
class SpatialGrid {
public:
    explicit SpatialGrid(float cellSize = 50.0f);

    void Insert(entt::entity entity, const glm::vec3& position);
    void Clear();

    std::vector<entt::entity> QueryRadius(const glm::vec3& position, float radius) const;

private:
    struct CellKey {
        int x, z;
        bool operator==(const CellKey& other) const { return x == other.x && z == other.z; }
    };
    struct CellKeyHash {
        size_t operator()(const CellKey& key) const {
            return std::hash<int>()(key.x) ^ (std::hash<int>()(key.z) << 1);
        }
    };

    CellKey GetCell(const glm::vec3& position) const;

    float m_cellSize;
    std::unordered_map<CellKey, std::vector<entt::entity>, CellKeyHash> m_cells;
};