#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "Shader.h"

// A single terrain chunk's heightfield: an NxN grid of world-space Y
// values, converted to a renderable mesh (regular grid, height-displaced,
// central-difference normals). Phase A only — no texturing (single flat
// material tint), no collision (Phase B), no multi-chunk seam blending
// (design doc §10 open question — a brush strictly clamped to one chunk
// for now).
class TerrainChunk {
public:
    TerrainChunk(int chunkX, int chunkZ, float chunkSize, int resolution);
    ~TerrainChunk();

    TerrainChunk(const TerrainChunk&) = delete;
    TerrainChunk& operator=(const TerrainChunk&) = delete;

    // Recomputes normals from the current height grid and re-uploads the
    // full VBO. Phase A always does a full re-upload on every edit (simple,
    // correct) — see design doc §8 on only optimizing this once profiling
    // shows it's actually a bottleneck.
    void RebuildMesh();
    void Draw(const Shader& shader) const;

    int GetResolution() const { return m_resolution; }
    float GetChunkSize() const { return m_chunkSize; }
    int ChunkX() const { return m_chunkX; }
    int ChunkZ() const { return m_chunkZ; }

    // ix/iz in [0, resolution). Height is Y in world space.
    float GetHeight(int ix, int iz) const;
    void SetHeight(int ix, int iz, float height);

    std::vector<float>& GetHeightsMutable() { return m_heights; }
    const std::vector<float>& GetHeights() const { return m_heights; }
    void SetHeights(std::vector<float> heights); // replaces wholesale (used by undo) — does NOT rebuild; call RebuildMesh() after

    // Converts a local grid index to world-space XZ (Y comes from GetHeight).
    glm::vec3 GridToWorld(int ix, int iz) const;

    // Bilinear height sample at an arbitrary world XZ position within this
    // chunk's bounds. Used by both rendering-adjacent brush application and
    // ray-vs-heightfield picking.
    float SampleHeightBilinear(float worldX, float worldZ) const;

    // World-space AABB of this chunk, accounting for actual min/max height
    // — used by TerrainSystem::RaycastTerrain to skip chunks the ray can't
    // possibly hit before doing the more expensive per-step march.
    void GetWorldBounds(glm::vec3& outMin, glm::vec3& outMax) const;

    bool ContainsWorldXZ(float worldX, float worldZ) const;

private:
    int m_chunkX, m_chunkZ;
    float m_chunkSize;
    int m_resolution;
    float m_spacing; // world units between adjacent grid samples

    std::vector<float> m_heights;   // resolution*resolution, row-major (iz*resolution+ix)
    std::vector<glm::vec3> m_normals;

    unsigned int m_VAO = 0, m_VBO = 0, m_EBO = 0;
    int m_indexCount = 0;
};