#include "ChunkManager.h"
#include "SceneLoader.h"
#include "../core/Log.h"
#include <cmath>

ChunkManager::ChunkManager(float chunkSize, int loadRadius)
    : m_chunkSize(chunkSize), m_loadRadius(loadRadius) {}

ChunkManager::ChunkKey ChunkManager::WorldToChunk(const glm::vec3& position) const {
    return {
        static_cast<int>(std::floor(position.x / m_chunkSize)),
        static_cast<int>(std::floor(position.z / m_chunkSize))
    };
}

std::string ChunkManager::GetChunkPath(int x, int z) const {
    return "scenes/chunks/chunk_" + std::to_string(x) + "_" + std::to_string(z) + ".json";
}

void ChunkManager::Update(const glm::vec3& viewerPosition, Scene& scene) {
    ChunkKey center = WorldToChunk(viewerPosition);

    std::unordered_set<ChunkKey, ChunkKeyHash> desiredChunks;
    for (int dx = -m_loadRadius; dx <= m_loadRadius; ++dx) {
        for (int dz = -m_loadRadius; dz <= m_loadRadius; ++dz) {
            desiredChunks.insert({ center.x + dx, center.z + dz });
        }
    }

    // Load chunks that should be active but aren't yet
    for (const auto& key : desiredChunks) {
        if (m_loadedChunks.find(key) == m_loadedChunks.end()) {
            SceneLoader::LoadChunk(GetChunkPath(key.x, key.z), scene, key.x, key.z);
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
        SceneLoader::UnloadChunk(scene, key.x, key.z);
        m_loadedChunks.erase(key);
    }
}