#pragma once
#include "Scene.h"
#include <unordered_set>
#include <string>

class PhysicsWorld; // forward declare

class ChunkManager {
public:
    ChunkManager(float chunkSize, int loadRadius);

    void Update(const glm::vec3& viewerPosition, Scene& scene, PhysicsWorld& physicsWorld, float maxRenderDistance);
    void ForceReloadAll(Scene& scene, PhysicsWorld& physicsWorld);
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
    std::string GetChunkPath(int x, int z) const;

    float m_chunkSize;
    int m_loadRadius; // in chunks, not world units
    int m_unloadRadius = m_loadRadius + 2;

    std::unordered_set<ChunkKey, ChunkKeyHash> m_loadedChunks;

    ChunkKey m_lastCenter{};
    bool m_hasLastCenter = false;
    
    // Added this line to track the slider state!
    float m_lastRenderDistance = -1.0f;
};