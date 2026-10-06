#pragma once
#include "Scene.h"
#include <string>
#include <unordered_map>
#include <memory>

class Model;        // forward declare
class PhysicsWorld; // forward declare

class SceneLoader {
public:
    static std::shared_ptr<Model> GetOrLoadModel(const std::string& path);
    static void LoadFromFile(const std::string& path, Scene& scene);
    static void LoadChunk(const std::string& path, Scene& scene, PhysicsWorld& physicsWorld, int chunkX, int chunkZ, float chunkSize);
    static void UnloadChunk(Scene& scene, PhysicsWorld& physicsWorld, int chunkX, int chunkZ);

    // Call BEFORE destroying any entity that might be chunk-tracked (editor
    // Delete key, gameplay destruction, etc.) so the change survives a
    // chunk unload/reload or a save/load. No-ops for entities that aren't
    // BuildingPlot/ChunkJsonIndex tagged (ground plates, roads, sidewalks,
    // pedestrians — nothing breaks calling it on those).
    static void RecordEntityDestructionDelta(Scene& scene, entt::entity entity);

private:
    static std::unordered_map<std::string, std::shared_ptr<Model>> s_modelCache;
};