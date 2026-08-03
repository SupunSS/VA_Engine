#include "ChunkManager.h"
#include "SceneLoader.h"
#include "../core/Log.h"
#include "../physics/PhysicsWorld.h"
#include <cmath>
#include <glm/gtx/norm.hpp>
#include "../core/AssetPaths.h"

ChunkManager::ChunkManager(float chunkSize, int loadRadius)
    : m_chunkSize(chunkSize), m_loadRadius(loadRadius), m_unloadRadius(loadRadius + 2) {}

ChunkManager::ChunkKey ChunkManager::WorldToChunk(const glm::vec3& position) const {
    // Removed the fake +0.1f epsilon. Real hysteresis is now handled in Update().
    return {
        static_cast<int>(std::floor(position.x / m_chunkSize)),
        static_cast<int>(std::floor(position.z / m_chunkSize))
    };
}

std::string ChunkManager::GetChunkPath(int x, int z) const {
    return AssetPaths::Resolve(AssetPaths::Category::Scenes,
        "chunks/chunk_" + std::to_string(x) + "_" + std::to_string(z) + ".json");
}

void ChunkManager::Update(const glm::vec3& viewerPosition, Scene& scene, PhysicsWorld& physicsWorld, float maxRenderDistance) {
    ChunkKey currentChunk = WorldToChunk(viewerPosition);
    bool chunkChanged = false;

    if (m_hasLastCenter) {
        // Calculate the exact center of our last recorded chunk in world space
        glm::vec2 lastCenterWorldPos(
            (m_lastCenter.x + 0.5f) * m_chunkSize,
            (m_lastCenter.z + 0.5f) * m_chunkSize
        );
        glm::vec2 viewerPos2D(viewerPosition.x, viewerPosition.z);
        
        // TRUE Hysteresis: Only shift the center if we move past the chunk's edge PLUS a 2-meter buffer
        float distToLastCenter = glm::distance(viewerPos2D, lastCenterWorldPos);
        float hysteresisThreshold = (m_chunkSize / 2.0f) + 2.0f; 

        if (distToLastCenter > hysteresisThreshold) {
            chunkChanged = true;
            m_lastCenter = currentChunk;
        }
    } else {
        m_lastCenter = currentChunk;
        m_hasLastCenter = true;
        chunkChanged = true;
    }

    // Early-out ONLY if we haven't crossed a hysteresis boundary AND the UI slider hasn't moved
    if (!chunkChanged && maxRenderDistance == m_lastRenderDistance) {
        return; 
    }
    
    // Save current slider state for the next frame
    m_lastRenderDistance = maxRenderDistance;

    // Use maxRenderDistance for desired loading, not m_loadRadius!
    const int renderDistanceInChunks = static_cast<int>(std::ceil(maxRenderDistance / m_chunkSize));
    const int unloadRadius = std::max(m_loadRadius + 2, renderDistanceInChunks + 2);

    std::unordered_set<ChunkKey, ChunkKeyHash> desiredChunks;
    // Iterate using renderDistanceInChunks so chunks actually load when you drag the UI slider
    for (int dx = -renderDistanceInChunks; dx <= renderDistanceInChunks; ++dx) {
        for (int dz = -renderDistanceInChunks; dz <= renderDistanceInChunks; ++dz) {
            desiredChunks.insert({ m_lastCenter.x + dx, m_lastCenter.z + dz });
        }
    }

    std::unordered_set<ChunkKey, ChunkKeyHash> keepAliveChunks;
    for (int dx = -unloadRadius; dx <= unloadRadius; ++dx) {
        for (int dz = -unloadRadius; dz <= unloadRadius; ++dz) {
            keepAliveChunks.insert({ m_lastCenter.x + dx, m_lastCenter.z + dz });
        }
    }

    for (const auto& key : desiredChunks) {
        if (m_loadedChunks.find(key) == m_loadedChunks.end()) {
            SceneLoader::LoadChunk(GetChunkPath(key.x, key.z), scene, physicsWorld, key.x, key.z, m_chunkSize);
            m_loadedChunks.insert(key);
        }
    }

    std::vector<ChunkKey> toUnload;
    for (const auto& key : m_loadedChunks) {
        if (keepAliveChunks.find(key) == keepAliveChunks.end()) {
            toUnload.push_back(key);
        }
    }

    for (const auto& key : toUnload) {
        SceneLoader::UnloadChunk(scene, physicsWorld, key.x, key.z);
        m_loadedChunks.erase(key);
    }
}

void ChunkManager::ForceReloadAll(Scene& scene, PhysicsWorld& physicsWorld) {
    for (const auto& key : m_loadedChunks) {
        SceneLoader::UnloadChunk(scene, physicsWorld, key.x, key.z);
    }
    m_loadedChunks.clear();
    m_hasLastCenter = false; // forces Update() to treat the next call as a fresh center, reloading everything
}