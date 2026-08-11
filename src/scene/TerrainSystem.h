#pragma once
#include "../rendering/TerrainChunk.h"
#include <glm/glm.hpp>
#include <unordered_map>
#include <unordered_set>
#include <memory>

class Shader;

class TerrainSystem {
public:
    enum class BrushType { Raise, Lower, Smooth };

    TerrainSystem(float chunkSize = 50.0f, int resolution = 33, int loadRadius = 4);

    // Independent of ChunkManager's own streaming — loads/unloads terrain
    // chunks in a radius around viewerPosition using the same chunk-grid
    // math, deliberately not sharing state with ChunkManager (see design
    // doc's §9 note on avoiding coupling to its private internals for
    // Phase A).
    void Update(const glm::vec3& viewerPosition);

    void Render(const Shader& shader, const glm::mat4& view, const glm::mat4& projection,
                const glm::vec3& viewPos, const glm::vec3& dirLightDir, const glm::vec3& dirLightColor) const;

    // Returns the chunk whose bounds contain worldX/worldZ, or nullptr if
    // none is currently loaded there.
    TerrainChunk* FindChunkAtWorldXZ(float worldX, float worldZ);

    // Simple fixed-step march + binary-search refine against each nearby
    // chunk's heightfield. Returns the closest hit along the ray, if any.
    bool RaycastTerrain(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                        glm::vec3& outHitPoint, TerrainChunk** outChunk) const;

    // Mutates heights within radius of worldHitPoint, strictly clamped to
    // the single chunk passed in — a stroke that crosses a chunk boundary
    // simply doesn't affect the neighbor (see design doc §10, multi-chunk
    // seams are an explicit known Phase A limitation, not a bug).
    void ApplyBrush(TerrainChunk& chunk, const glm::vec3& worldHitPoint, BrushType type,
                     float radius, float strength, float deltaTime);

    float GetChunkSize() const { return m_chunkSize; }
    int GetResolution() const { return m_resolution; }

private:
    struct ChunkKey {
        int x, z;
        bool operator==(const ChunkKey& other) const { return x == other.x && z == other.z; }
    };
    struct ChunkKeyHash {
        size_t operator()(const ChunkKey& key) const {
            return std::hash<int>()(key.x) ^ (std::hash<int>()(key.z) << 1);
        }
    };

    ChunkKey WorldToChunk(const glm::vec3& position) const;

    float m_chunkSize;
    int m_resolution;
    int m_loadRadius;

    std::unordered_map<ChunkKey, std::unique_ptr<TerrainChunk>, ChunkKeyHash> m_chunks;
};