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

    // chunkSize sizes the per-chunk ground plate spawned alongside whatever
    // entities the chunk JSON defines — keeps the player from falling
    // through gaps between streamed-in areas.
    static void LoadChunk(const std::string& path, Scene& scene, PhysicsWorld& physicsWorld, int chunkX, int chunkZ, float chunkSize);
    static void UnloadChunk(Scene& scene, PhysicsWorld& physicsWorld, int chunkX, int chunkZ);

private:
    // Cache so identical model paths share one GPU-side Model instance
    static std::unordered_map<std::string, std::shared_ptr<Model>> s_modelCache;
};