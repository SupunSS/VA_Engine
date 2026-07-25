#include "ChunkManager.h"
#include "SceneLoader.h"
#include "../core/Log.h"
#include "../physics/PhysicsWorld.h"
#include <cmath>

ChunkManager::ChunkManager(float chunkSize, int loadRadius)
    : m_chunkSize(chunkSize), m_loadRadius(loadRadius) {}

ChunkManager::ChunkKey ChunkManager::WorldToChunk(const glm::vec3& position) const {
    // Small epsilon pulls the effective chunk boundary slightly inward on
    // both sides, so floating-point jitter of a few millimeters around an
    // exact chunk-size multiple (extremely common for a "resting" physics
    // body — Jolt, like every physics engine, allows sub-millimeter jitter
    // at rest) doesn't flip the computed chunk index back and forth every
    // frame. Without this, a stationary player standing near a chunk
    // boundary causes ChunkManager::Update to see a "new center" almost
    // every frame, which tears down and rebuilds the entire boundary ring
    // of chunks repeatedly — this is what was causing buildings to
    // intermittently vanish and reappear while just standing still.
    constexpr float kBoundaryEpsilon = 0.1f;
    return {
        static_cast<int>(std::floor((position.x + kBoundaryEpsilon) / m_chunkSize)),
        static_cast<int>(std::floor((position.z + kBoundaryEpsilon) / m_chunkSize))
    };
}

std::string ChunkManager::GetChunkPath(int x, int z) const {
    return "scenes/chunks/chunk_" + std::to_string(x) + "_" + std::to_string(z) + ".json";
}

void ChunkManager::Update(const glm::vec3& viewerPosition, Scene& scene, PhysicsWorld& physicsWorld) {
    ChunkKey center = WorldToChunk(viewerPosition);

    // Nothing to do if the viewer is still in the same chunk as last frame —
    // at a large loadRadius this desired-set computation is (2*radius+1)^2
    // hash-set insertions plus a full scan of m_loadedChunks, done every
    // single frame regardless of movement. That's pure waste most frames,
    // since the viewer only actually crosses a chunk boundary occasionally.
    if (m_hasLastCenter && center.x == m_lastCenter.x && center.z == m_lastCenter.z) {
        return;
    }
    m_lastCenter = center;
    m_hasLastCenter = true;

    std::unordered_set<ChunkKey, ChunkKeyHash> desiredChunks;
    for (int dx = -m_loadRadius; dx <= m_loadRadius; ++dx) {
        for (int dz = -m_loadRadius; dz <= m_loadRadius; ++dz) {
            desiredChunks.insert({ center.x + dx, center.z + dz });
        }
    }

    // Load chunks that should be active but aren't yet
    for (const auto& key : desiredChunks) {
        if (m_loadedChunks.find(key) == m_loadedChunks.end()) {
            SceneLoader::LoadChunk(GetChunkPath(key.x, key.z), scene, physicsWorld, key.x, key.z, m_chunkSize);
            m_loadedChunks.insert(key);
        }
    }

    // Unload chunks that are no longer needed
    std::vector<ChunkKey> toUnload;
    for (const auto& key : m_loadedChunks) {
        if (desiredChunks.find(key) == desiredChunks.end()) {
            toUnload.push_back(key);
        }
    }

    for (const auto& key : toUnload) {
        SceneLoader::UnloadChunk(scene, physicsWorld, key.x, key.z);
        m_loadedChunks.erase(key);
    }
}