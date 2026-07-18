#include "SceneLoader.h"
#include "Components.h"
#include "../rendering/Model.h"
#include "../physics/PhysicsWorld.h"
#include "../core/Log.h"
#include "../core/Assert.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>

std::unordered_map<std::string, std::shared_ptr<Model>> SceneLoader::s_modelCache;

namespace {
// Thickness of the per-chunk ground plate. Top surface sits at y = 0 to
// match where CharacterController's spawn point and existing test bodies
// already assume the ground is. Made generously thick (not just a thin
// slab) as a safety margin against the one-frame lag between a respawn
// and ChunkManager::Update picking up the new viewer position — if the
// character falls a bit before the ground body registers, a thin slab
// can let it fall clean through before ever touching it.
constexpr float kGroundThickness = 10.0f;
}

std::shared_ptr<Model> SceneLoader::GetOrLoadModel(const std::string& path) {
    auto it = s_modelCache.find(path);
    if (it != s_modelCache.end()) {
        return it->second;
    }

    auto model = std::make_shared<Model>(path);
    s_modelCache[path] = model;
    return model;
}

void SceneLoader::LoadFromFile(const std::string& path, Scene& scene) {
    std::ifstream file(path);
    if (!file.is_open()) {
        Log::Warn("Failed to open scene file: {}", path);
        return;
    }

    nlohmann::json data;
    file >> data;

    int count = 0;
    for (const auto& entry : data["entities"]) {
        std::string modelPath = entry["model"];
        auto pos = entry["position"];

        auto model = GetOrLoadModel(modelPath);

        auto entity = scene.CreateEntity();
        scene.Registry.get<Transform>(entity).Position =
            glm::vec3(pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>());
        scene.Registry.emplace<MeshRenderer>(entity, model);

        count++;
    }

    Log::Info("Scene loaded from {}: {} entities", path, count);
}

void SceneLoader::LoadChunk(const std::string& path, Scene& scene, PhysicsWorld& physicsWorld, int chunkX, int chunkZ, float chunkSize) {
    // Ground plate for this chunk — same footprint ChunkManager streams
    // (chunkSize x chunkSize), tagged with ChunkId so UnloadChunk cleans it
    // up (visual + physics body) along with everything else. Created FIRST,
    // unconditionally — a chunk with no JSON entity file still needs ground,
    // otherwise it's a hole in the floor.
    {
        const glm::vec3 halfExtents(chunkSize * 0.5f, kGroundThickness * 0.5f, chunkSize * 0.5f);
        const glm::vec3 center(
            chunkX * chunkSize + chunkSize * 0.5f,
            -kGroundThickness * 0.5f,
            chunkZ * chunkSize + chunkSize * 0.5f
        );

        auto groundEntity = scene.CreateEntity();
        auto& groundTransform = scene.Registry.get<Transform>(groundEntity);
        groundTransform.Position = center;
        groundTransform.Scale = halfExtents; // cube.obj spans -1..1, so Scale == halfExtents directly

        auto groundModel = GetOrLoadModel("models/cube.obj");
        scene.Registry.emplace<MeshRenderer>(groundEntity, groundModel);

        const auto groundBodyId = physicsWorld.CreateBoxBody(center, halfExtents, /*isStatic=*/true);
        scene.Registry.emplace<RigidBody>(groundEntity, groundBodyId, true, PhysicsShapeType::Box, halfExtents, 0.5f);
        scene.Registry.emplace<ChunkId>(groundEntity, ChunkId{ chunkX, chunkZ });
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        Log::Warn("Chunk file not found: {} (ground plate still created)", path);
        return;
    }

    nlohmann::json data;
    file >> data;

    int count = 0;
    for (const auto& entry : data["entities"]) {
        std::string modelPath = entry["model"];
        auto pos = entry["position"];

        auto model = GetOrLoadModel(modelPath);

        auto entity = scene.CreateEntity();
        scene.Registry.get<Transform>(entity).Position =
            glm::vec3(pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>());
        scene.Registry.emplace<MeshRenderer>(entity, model);
        scene.Registry.emplace<ChunkId>(entity, ChunkId{ chunkX, chunkZ });

        count++;
    }

    Log::Info("Chunk ({}, {}) loaded: {} entities + ground plate", chunkX, chunkZ, count);
}

void SceneLoader::UnloadChunk(Scene& scene, PhysicsWorld& physicsWorld, int chunkX, int chunkZ) {
    std::vector<entt::entity> toDestroy;

    auto view = scene.Registry.view<ChunkId>();
    for (auto entity : view) {
        const auto& chunk = view.get<ChunkId>(entity);
        if (chunk.x == chunkX && chunk.z == chunkZ) {
            toDestroy.push_back(entity);
        }
    }

    for (auto entity : toDestroy) {
        // Destroy the physics body before the ECS entity — this is the fix
        // for the leak: previously chunk entities with a RigidBody (like the
        // ground plate) had their Jolt body silently orphaned every unload.
        if (scene.Registry.all_of<RigidBody>(entity)) {
            const auto& rigidBody = scene.Registry.get<RigidBody>(entity);
            if (!rigidBody.BodyId.IsInvalid()) {
                physicsWorld.DestroyBody(rigidBody.BodyId);
            }
        }
        scene.DestroyEntity(entity);
    }

    Log::Info("Chunk ({}, {}) unloaded: {} entities", chunkX, chunkZ, toDestroy.size());
}