#include "SpatialGrid.h"
#include <cmath>

SpatialGrid::SpatialGrid(float cellSize) : m_cellSize(cellSize) {}

SpatialGrid::CellKey SpatialGrid::GetCell(const glm::vec3& position) const {
    return { static_cast<int>(std::floor(position.x / m_cellSize)),
             static_cast<int>(std::floor(position.z / m_cellSize)) };
}

void SpatialGrid::Insert(entt::entity entity, const glm::vec3& position) {
    m_cells[GetCell(position)].push_back(entity);
}

void SpatialGrid::Clear() {
    m_cells.clear();
}

std::vector<entt::entity> SpatialGrid::QueryRadius(const glm::vec3& position, float radius) const {
    std::vector<entt::entity> result;
    int cellRadius = static_cast<int>(std::ceil(radius / m_cellSize));
    CellKey center = GetCell(position);

    for (int dx = -cellRadius; dx <= cellRadius; ++dx) {
        for (int dz = -cellRadius; dz <= cellRadius; ++dz) {
            auto it = m_cells.find({ center.x + dx, center.z + dz });
            if (it != m_cells.end()) {
                result.insert(result.end(), it->second.begin(), it->second.end());
            }
        }
    }
    return result;
}