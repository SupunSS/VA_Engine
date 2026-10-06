#include "TerrainChunk.h"
#include <glad/glad.h>
#include <algorithm>
#include <cstddef>

namespace {
struct TerrainVertex {
    glm::vec3 Position;
    glm::vec3 Normal;
};
}

TerrainChunk::TerrainChunk(int chunkX, int chunkZ, float chunkSize, int resolution)
    : m_chunkX(chunkX), m_chunkZ(chunkZ), m_chunkSize(chunkSize), m_resolution(resolution) {
    m_spacing = m_chunkSize / static_cast<float>(m_resolution - 1);
    m_heights.assign(static_cast<size_t>(m_resolution) * m_resolution, 0.0f); // flat start — Phase A
    m_normals.assign(static_cast<size_t>(m_resolution) * m_resolution, glm::vec3(0.0f, 1.0f, 0.0f));

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    RebuildMesh();
}

TerrainChunk::~TerrainChunk() {
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    if (m_EBO) glDeleteBuffers(1, &m_EBO);
}

float TerrainChunk::GetHeight(int ix, int iz) const {
    ix = std::clamp(ix, 0, m_resolution - 1);
    iz = std::clamp(iz, 0, m_resolution - 1);
    return m_heights[static_cast<size_t>(iz) * m_resolution + ix];
}

void TerrainChunk::SetHeight(int ix, int iz, float height) {
    if (ix < 0 || ix >= m_resolution || iz < 0 || iz >= m_resolution) return;
    m_heights[static_cast<size_t>(iz) * m_resolution + ix] = height;
}

void TerrainChunk::SetHeights(std::vector<float> heights) {
    if (heights.size() == m_heights.size()) {
        m_heights = std::move(heights);
    }
}

glm::vec3 TerrainChunk::GridToWorld(int ix, int iz) const {
    return glm::vec3(
        m_chunkX * m_chunkSize + ix * m_spacing,
        GetHeight(ix, iz),
        m_chunkZ * m_chunkSize + iz * m_spacing
    );
}

bool TerrainChunk::ContainsWorldXZ(float worldX, float worldZ) const {
    const float minX = m_chunkX * m_chunkSize;
    const float minZ = m_chunkZ * m_chunkSize;
    return worldX >= minX && worldX <= minX + m_chunkSize &&
           worldZ >= minZ && worldZ <= minZ + m_chunkSize;
}

float TerrainChunk::SampleHeightBilinear(float worldX, float worldZ) const {
    const float localX = (worldX - m_chunkX * m_chunkSize) / m_spacing;
    const float localZ = (worldZ - m_chunkZ * m_chunkSize) / m_spacing;

    const int ix0 = std::clamp(static_cast<int>(std::floor(localX)), 0, m_resolution - 1);
    const int iz0 = std::clamp(static_cast<int>(std::floor(localZ)), 0, m_resolution - 1);
    const int ix1 = std::min(ix0 + 1, m_resolution - 1);
    const int iz1 = std::min(iz0 + 1, m_resolution - 1);

    const float fx = std::clamp(localX - ix0, 0.0f, 1.0f);
    const float fz = std::clamp(localZ - iz0, 0.0f, 1.0f);

    const float h00 = GetHeight(ix0, iz0);
    const float h10 = GetHeight(ix1, iz0);
    const float h01 = GetHeight(ix0, iz1);
    const float h11 = GetHeight(ix1, iz1);

    const float hx0 = glm::mix(h00, h10, fx);
    const float hx1 = glm::mix(h01, h11, fx);
    return glm::mix(hx0, hx1, fz);
}

void TerrainChunk::GetWorldBounds(glm::vec3& outMin, glm::vec3& outMax) const {
    const float minY = *std::min_element(m_heights.begin(), m_heights.end());
    const float maxY = *std::max_element(m_heights.begin(), m_heights.end());
    outMin = glm::vec3(m_chunkX * m_chunkSize, minY, m_chunkZ * m_chunkSize);
    outMax = glm::vec3(m_chunkX * m_chunkSize + m_chunkSize, maxY, m_chunkZ * m_chunkSize + m_chunkSize);
}

void TerrainChunk::RebuildMesh() {
    // Central-difference normal recompute — matches the design doc's §4.1
    // description and the standard technique for heightmap terrain.
    m_normals.assign(static_cast<size_t>(m_resolution) * m_resolution, glm::vec3(0.0f));
    for (int iz = 0; iz < m_resolution; ++iz) {
        for (int ix = 0; ix < m_resolution; ++ix) {
            const float hL = GetHeight(ix - 1, iz);
            const float hR = GetHeight(ix + 1, iz);
            const float hD = GetHeight(ix, iz - 1);
            const float hU = GetHeight(ix, iz + 1);
            const glm::vec3 normal = glm::normalize(glm::vec3(hL - hR, 2.0f * m_spacing, hD - hU));
            m_normals[static_cast<size_t>(iz) * m_resolution + ix] = normal;
        }
    }

    std::vector<TerrainVertex> vertices;
    vertices.reserve(static_cast<size_t>(m_resolution) * m_resolution);
    for (int iz = 0; iz < m_resolution; ++iz) {
        for (int ix = 0; ix < m_resolution; ++ix) {
            TerrainVertex v;
            v.Position = GridToWorld(ix, iz);
            v.Normal = m_normals[static_cast<size_t>(iz) * m_resolution + ix];
            vertices.push_back(v);
        }
    }

    std::vector<unsigned int> indices;
    indices.reserve(static_cast<size_t>(m_resolution - 1) * (m_resolution - 1) * 6);
    for (int iz = 0; iz < m_resolution - 1; ++iz) {
        for (int ix = 0; ix < m_resolution - 1; ++ix) {
            const unsigned int i0 = iz * m_resolution + ix;
            const unsigned int i1 = i0 + 1;
            const unsigned int i2 = i0 + m_resolution;
            const unsigned int i3 = i2 + 1;
            indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
            indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
        }
    }
    m_indexCount = static_cast<int>(indices.size());

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(TerrainVertex), vertices.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, Position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, Normal));

    glBindVertexArray(0);
}

void TerrainChunk::Draw(const Shader& shader) const {
    (void)shader; // shader is already bound by TerrainSystem::Render before this is called
    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}