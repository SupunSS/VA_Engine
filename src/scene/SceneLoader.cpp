#include "SceneLoader.h"
#include "Components.h"
#include "../rendering/Model.h"
#include "../core/Log.h"
#include "../core/Assert.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>

std::unordered_map<std::string, std::shared_ptr<Model>> SceneLoader::s_modelCache;

void SceneLoader::LoadFromFile(const std::string& path, Scene& scene) {
    std::ifstream file(path);
    ENGINE_ASSERT(file.is_open(), "Failed to open scene file");

    nlohmann::json data;
    file >> data;

    int count = 0;
    for (const auto& entry : data["entities"]) {
        std::string modelPath = entry["model"];
        auto pos = entry["position"];

        // Reuse Model if we've already loaded this path — avoids re-uploading
        // identical GPU data for every instance of the same asset.
        if (s_modelCache.find(modelPath) == s_modelCache.end()) {
            s_modelCache[modelPath] = std::make_shared<Model>(modelPath);
        }

        auto entity = scene.CreateEntity();
        scene.Registry.get<Transform>(entity).Position = 
            glm::vec3(pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>());
        scene.Registry.emplace<MeshRenderer>(entity, s_modelCache[modelPath]);

        count++;
    }

    Log::Info("Scene loaded from {}: {} entities", path, count);
}

void SceneLoader::LoadChunk(const std::string& path, Scene& scene, int chunkX, int chunkZ) {
    std::ifstream file(path);
    if (!file.is_open()) {
        Log::Warn("Chunk file not found: {}", path);
        return;
    }

    nlohmann::json data;
    file >> data;

    int count = 0;
    for (const auto& entry : data["entities"]) {
        std::string modelPath = entry["model"];
        auto pos = entry["position"];

        if (s_modelCache.find(modelPath) == s_modelCache.end()) {
            s_modelCache[modelPath] = std::make_shared<Model>(modelPath);
        }

        auto entity = scene.CreateEntity();
        scene.Registry.get<Transform>(entity).Position =
            glm::vec3(pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>());
        scene.Registry.emplace<MeshRenderer>(entity, s_modelCache[modelPath]);
        scene.Registry.emplace<ChunkId>(entity, ChunkId{ chunkX, chunkZ });

        count++;
    }

    Log::Info("Chunk ({}, {}) loaded: {} entities", chunkX, chunkZ, count);
}

void SceneLoader::UnloadChunk(Scene& scene, int chunkX, int chunkZ) {
    std::vector<entt::entity> toDestroy;

    auto view = scene.Registry.view<ChunkId>();
    for (auto entity : view) {
        const auto& chunk = view.get<ChunkId>(entity);
        if (chunk.x == chunkX && chunk.z == chunkZ) {
            toDestroy.push_back(entity);
        }
    }

    for (auto entity : toDestroy) {
        scene.DestroyEntity(entity);
    }

    Log::Info("Chunk ({}, {}) unloaded: {} entities", chunkX, chunkZ, toDestroy.size());
}