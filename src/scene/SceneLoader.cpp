#include "SceneLoader.h"
#include "Components.h"
#include "../rendering/Model.h"
#include "../rendering/Material.h"
#include "../physics/PhysicsWorld.h"
#include "../core/Log.h"
#include "../core/Assert.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>
#include "CityLayoutConfig.h"
#include "CityLayout.h"
#include "../rendering/Animator.h"
#include "ChunkDeltaStore.h"
#include "../core/AssetPaths.h"

std::unordered_map<std::string, std::shared_ptr<Model>> SceneLoader::s_modelCache;

namespace {


std::shared_ptr<Material> GetGroundMaterial() {
    static std::shared_ptr<Material> groundMaterial = [] {
        auto material = std::make_shared<Material>();
        material->albedoTint = glm::vec3(0.5f, 0.5f, 0.5f);
        return material;
    }();
    return groundMaterial;
}



// Road/sidewalk surfaces are embedded slightly below y=0 and poke up just
// slightly above it — guarantees no floating gap above the ground plate's
// top (which sits exactly at y=0) and no z-fighting with it, without
// needing razor-thin boxes.
constexpr float kRoadTopY = 0.05f;
constexpr float kRoadBottomY = -0.15f;
constexpr float kSidewalkTopY = 0.15f; // sits above the road like a real curb
constexpr float kSidewalkBottomY = -0.15f;


uint32_t HashCoords(int x, int z, int salt) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u
                ^ static_cast<uint32_t>(z) * 668265263u
                ^ static_cast<uint32_t>(salt) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= (h >> 16);
    return h;
}

float HashToUnitFloat(int x, int z, int salt) {
    return (HashCoords(x, z, salt) & 0xFFFFFFu) / float(0xFFFFFFu);
}

std::shared_ptr<Material> GetRoadMaterial() {
    static std::shared_ptr<Material> material = [] {
        auto m = std::make_shared<Material>();
        m->albedoTint = glm::vec3(0.12f, 0.12f, 0.13f); // dark asphalt
        return m;
    }();
    return material;
}

std::shared_ptr<Material> GetSidewalkMaterial() {
    static std::shared_ptr<Material> material = [] {
        auto m = std::make_shared<Material>();
        m->albedoTint = glm::vec3(0.55f, 0.55f, 0.52f); // light concrete
        return m;
    }();
    return material;
}

// Small fixed palette rather than one Material per building — keeps visual
// variety without allocating an unbounded number of Material objects as
// chunks stream in and out repeatedly.
std::shared_ptr<Material> GetBuildingMaterial(uint32_t bucket) {
    static std::vector<std::shared_ptr<Material>> palette = [] {
        std::vector<std::shared_ptr<Material>> materials;
        const glm::vec3 tints[] = {
            glm::vec3(0.62f, 0.58f, 0.52f), // tan
            glm::vec3(0.45f, 0.47f, 0.50f), // blue-gray
            glm::vec3(0.55f, 0.50f, 0.45f), // brownish
            glm::vec3(0.40f, 0.40f, 0.40f), // neutral gray
            glm::vec3(0.58f, 0.54f, 0.60f), // muted purple-gray
        };
        for (const auto& tint : tints) {
            auto m = std::make_shared<Material>();
            m->albedoTint = tint;
            materials.push_back(m);
        }
        return materials;
    }();
    return palette[bucket % palette.size()];
}

void CreateFlatBox(Scene& scene, const glm::vec3& center, const glm::vec3& halfExtents,
                    std::shared_ptr<Material> material, int chunkX, int chunkZ) {
    auto entity = scene.CreateEntity();
    auto& transform = scene.Registry.get<Transform>(entity);
    transform.Position = center;
    transform.Scale = halfExtents; // cube.obj spans -1..1, so Scale == halfExtents directly
    scene.Registry.emplace<MeshRenderer>(entity, SceneLoader::GetOrLoadModel("cube.obj"), std::move(material));
    scene.Registry.emplace<ChunkId>(entity, ChunkId{ chunkX, chunkZ });
}

// Convenience wrapper for road/sidewalk strips, specified by their top/bottom
// Y extent rather than center+halfExtent directly — makes the "embedded
// slightly below 0, pokes up slightly above" placement easier to read at
// the call sites below.
void CreateSurfaceBox(Scene& scene, float centerX, float centerZ, float halfExtentX, float halfExtentZ,
                       float topY, float bottomY, std::shared_ptr<Material> material, int chunkX, int chunkZ) {
    const float centerY = (topY + bottomY) * 0.5f;
    const float halfExtentY = (topY - bottomY) * 0.5f;
    CreateFlatBox(scene, glm::vec3(centerX, centerY, centerZ), glm::vec3(halfExtentX, halfExtentY, halfExtentZ),
                  std::move(material), chunkX, chunkZ);
}

void GenerateCityBlock(Scene& scene, PhysicsWorld& physicsWorld, int chunkX, int chunkZ, float chunkSize) {
    const auto& config = CityLayoutConfig::Get();
    const float worldX0 = chunkX * chunkSize;
    const float worldZ0 = chunkZ * chunkSize;
    const float worldXCenter = worldX0 + chunkSize * 0.5f;
    const float worldZCenter = worldZ0 + chunkSize * 0.5f;

    // --- Road ring: 4 strips along the chunk edges. Corners double-cover —
    // harmless, same flat color, coplanar. ------------------------------
    CreateSurfaceBox(scene, worldXCenter, worldZ0 + config.RoadWidth * 0.5f,
                      chunkSize * 0.5f, config.RoadWidth * 0.5f, kRoadTopY, kRoadBottomY, GetRoadMaterial(), chunkX, chunkZ);
    CreateSurfaceBox(scene, worldXCenter, worldZ0 + chunkSize - config.RoadWidth * 0.5f,
                      chunkSize * 0.5f, config.RoadWidth * 0.5f, kRoadTopY, kRoadBottomY, GetRoadMaterial(), chunkX, chunkZ);
    CreateSurfaceBox(scene, worldX0 + config.RoadWidth * 0.5f, worldZCenter,
                      config.RoadWidth * 0.5f, chunkSize * 0.5f, kRoadTopY, kRoadBottomY, GetRoadMaterial(), chunkX, chunkZ);
    CreateSurfaceBox(scene, worldX0 + chunkSize - config.RoadWidth * 0.5f, worldZCenter,
                      config.RoadWidth * 0.5f, chunkSize * 0.5f, kRoadTopY, kRoadBottomY, GetRoadMaterial(), chunkX, chunkZ);

    // --- Sidewalk ring: just inside the road ring -----------------------
    const float sidewalkInset = config.RoadWidth;
    const float sidewalkSpan = chunkSize - 2.0f * sidewalkInset;
    CreateSurfaceBox(scene, worldXCenter, worldZ0 + sidewalkInset + config.SidewalkWidth * 0.5f,
                      sidewalkSpan * 0.5f, config.SidewalkWidth * 0.5f, kSidewalkTopY, kSidewalkBottomY, GetSidewalkMaterial(), chunkX, chunkZ);
    CreateSurfaceBox(scene, worldXCenter, worldZ0 + chunkSize - sidewalkInset - config.SidewalkWidth * 0.5f,
                      sidewalkSpan * 0.5f, config.SidewalkWidth * 0.5f, kSidewalkTopY, kSidewalkBottomY, GetSidewalkMaterial(), chunkX, chunkZ);
    CreateSurfaceBox(scene, worldX0 + sidewalkInset + config.SidewalkWidth * 0.5f, worldZCenter,
                      config.SidewalkWidth * 0.5f, sidewalkSpan * 0.5f, kSidewalkTopY, kSidewalkBottomY, GetSidewalkMaterial(), chunkX, chunkZ);
    CreateSurfaceBox(scene, worldX0 + chunkSize - sidewalkInset - config.SidewalkWidth * 0.5f, worldZCenter,
                      config.SidewalkWidth * 0.5f, sidewalkSpan * 0.5f, kSidewalkTopY, kSidewalkBottomY, GetSidewalkMaterial(), chunkX, chunkZ);

    // --- Buildings: block-out boxes, real physics colliders --------------
    const float interiorMargin = config.RoadWidth + config.SidewalkWidth;
    const float interiorSpan = chunkSize - 2.0f * interiorMargin;
    const float plotSize = interiorSpan / static_cast<float>(config.BuildingGridSize);

    for (int plotX = 0; plotX < config.BuildingGridSize; ++plotX) {
        for (int plotZ = 0; plotZ < config.BuildingGridSize; ++plotZ) {
            const float plotCenterX = worldX0 + interiorMargin + plotSize * (plotX + 0.5f);
            const float plotCenterZ = worldZ0 + interiorMargin + plotSize * (plotZ + 0.5f);

            const float footprintHalf = (plotSize * 0.5f) - config.BuildingMargin;
            if (footprintHalf <= 0.5f) {
                continue; // margins too large for this plot size — skip rather than produce a degenerate building
            }

            const ChunkDelta* delta = ChunkDeltaStore::Get().GetDelta(chunkX, chunkZ);
            const int64_t plotKey = ChunkDeltaStore::PackCoord(plotX, plotZ);
            if (delta && delta->DestroyedBuildingPlots.count(plotKey)) {
                continue; // this building was destroyed in a previous session
            }

            const int plotSeed = plotX * 1000 + plotZ;
            const float heightT = HashToUnitFloat(chunkX, chunkZ, plotSeed);
            const float buildingHeight = config.BuildingMinHeight + heightT * (config.BuildingMaxHeight - config.BuildingMinHeight);
            const uint32_t colorBucket = HashCoords(chunkX, chunkZ, plotSeed + 7919); // different salt so height/color don't correlate

            const glm::vec3 halfExtents(footprintHalf, buildingHeight * 0.5f, footprintHalf);
            const glm::vec3 center(plotCenterX, buildingHeight * 0.5f, plotCenterZ);

            auto entity = scene.CreateEntity();
            auto& transform = scene.Registry.get<Transform>(entity);
            transform.Position = center;
            transform.Scale = halfExtents;

            scene.Registry.emplace<MeshRenderer>(entity, SceneLoader::GetOrLoadModel("cube.obj"), GetBuildingMaterial(colorBucket));

            const auto bodyId = physicsWorld.CreateBoxBody(center, halfExtents, /*isStatic=*/true);
            scene.Registry.emplace<RigidBody>(entity, bodyId, true, PhysicsShapeType::Box, halfExtents, 0.5f);
            scene.Registry.emplace<ChunkId>(entity, ChunkId{ chunkX, chunkZ });
            scene.Registry.emplace<BuildingPlot>(entity, BuildingPlot{ plotX, plotZ });
        }
    }
}
} // namespace

void SpawnPedestrians(Scene& scene, int chunkX, int chunkZ, float chunkSize) {
    const auto& config = CityLayoutConfig::Get();
    if (config.PedestriansPerChunk <= 0) {
        return;
    }

    auto waypoints = CityLayout::GetSidewalkLoopWaypoints(chunkX, chunkZ, chunkSize);
    if (waypoints.empty()) {
        return;
    }

    // Reusing the player skeleton/clips as a placeholder NPC — swap in a
    // dedicated pedestrian model+animations once one exists.
    auto pedestrianModel = SceneLoader::GetOrLoadModel("player/player.fbx");
    auto idleAnim = pedestrianModel->LoadAnimation(AssetPaths::Resolve(AssetPaths::Category::Models, "player/Idle.fbx"));
    auto walkAnim = pedestrianModel->LoadAnimation(AssetPaths::Resolve(AssetPaths::Category::Models, "player/Walking.fbx"));

    const int waypointCount = static_cast<int>(waypoints.size());
    for (int i = 0; i < config.PedestriansPerChunk; ++i) {
        auto entity = scene.CreateEntity();

        // Stagger starting waypoints so pedestrians in the same chunk don't
        // all spawn stacked on top of each other at index 0.
        const int startIndex = (i * waypointCount) / config.PedestriansPerChunk;

        auto& transform = scene.Registry.get<Transform>(entity);
        transform.Position = waypoints[startIndex];
        transform.Scale = glm::vec3(0.01f); // matches playerVisualTransform's scale in main.cpp — same source model/units

        scene.Registry.emplace<MeshRenderer>(entity, pedestrianModel, nullptr);
        scene.Registry.emplace<PedestrianTag>(entity);
        scene.Registry.emplace<ChunkId>(entity, ChunkId{ chunkX, chunkZ });

        auto& ai = scene.Registry.emplace<PedestrianAI>(entity);
        ai.PathWaypoints = waypoints;
        ai.CurrentWaypointIndex = startIndex;
        ai.MoveSpeed = config.PedestrianMoveSpeed;

        auto& animComp = scene.Registry.emplace<AnimatorComponent>(entity);
        animComp.AnimatorPtr = std::make_shared<Animator>();
        animComp.IdleAnim = idleAnim;
        animComp.WalkAnim = walkAnim;
        animComp.AnimatorPtr->PlayAnimation(idleAnim);
    }
}

void SceneLoader::RecordEntityDestructionDelta(Scene& scene, entt::entity entity) {
    if (entity == entt::null || !scene.Registry.valid(entity)) {
        return;
    }

    const auto* chunk = scene.Registry.try_get<ChunkId>(entity);
    if (!chunk) {
        return;
    }

    if (const auto* plot = scene.Registry.try_get<BuildingPlot>(entity)) {
        ChunkDeltaStore::Get().RecordBuildingDestroyed(chunk->x, chunk->z, plot->PlotX, plot->PlotZ);
    }

    if (const auto* jsonIndex = scene.Registry.try_get<ChunkJsonIndex>(entity)) {
        ChunkDeltaStore::Get().RecordJsonEntityDestroyed(chunk->x, chunk->z, jsonIndex->Index);
    }
}

std::shared_ptr<Model> SceneLoader::GetOrLoadModel(const std::string& path) {
    // Resolved here, once, so every caller (SceneLoader internally,
    // PedestrianSpawnSystem, EditorUI's asset browser, main.cpp) benefits
    // automatically — no need to wrap every call site individually.
    // Already-resolved/absolute paths (e.g. from JSON scene files that
    // already say "models/x.obj", or full paths from the editor) pass
    // through unchanged.
    const std::string resolvedPath = AssetPaths::Resolve(AssetPaths::Category::Models, path);

    auto it = s_modelCache.find(resolvedPath);
    if (it != s_modelCache.end()) {
        return it->second;
    }

    auto model = std::make_shared<Model>(resolvedPath);
    s_modelCache[resolvedPath] = model;
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
        const auto& config = CityLayoutConfig::Get();
        const glm::vec3 halfExtents(chunkSize * 0.5f, config.GroundThickness * 0.5f, chunkSize * 0.5f);
        const glm::vec3 center(
            chunkX * chunkSize + chunkSize * 0.5f,
            -config.GroundThickness * 0.5f,
            chunkZ * chunkSize + chunkSize * 0.5f
        );

        auto groundEntity = scene.CreateEntity();
        auto& groundTransform = scene.Registry.get<Transform>(groundEntity);
        groundTransform.Position = center;
        groundTransform.Scale = halfExtents;

        auto groundModel = GetOrLoadModel("cube.obj");
        scene.Registry.emplace<MeshRenderer>(groundEntity, groundModel, GetGroundMaterial());

        const auto groundBodyId = physicsWorld.CreateBoxBody(center, halfExtents, /*isStatic=*/true);
        scene.Registry.emplace<RigidBody>(groundEntity, groundBodyId, true, PhysicsShapeType::Box, halfExtents, 0.5f);
        scene.Registry.emplace<ChunkId>(groundEntity, ChunkId{ chunkX, chunkZ });
    }

    // Procedural city block — road ring, sidewalk ring, buildings. See the
    // GenerateCityBlock comment block above for the layout rationale.
    // Unconditional, same as the ground plate — works whether or not this
    // chunk also has a hand-authored JSON entity file below.
    GenerateCityBlock(scene, physicsWorld, chunkX, chunkZ, chunkSize);

    // Deltas recorded for this chunk in a previous session (destroyed/moved
    // hand-placed entities, runtime-spawned extras). Building-plot deltas
    // are looked up separately inside GenerateCityBlock above.
    const ChunkDelta* delta = ChunkDeltaStore::Get().GetDelta(chunkX, chunkZ);

    std::ifstream file(path);
    if (!file.is_open()) {
        Log::Warn("Chunk file not found: {} (ground plate + city block still created)", path);
    } else {
        nlohmann::json data;
        file >> data;

        int count = 0;
        int jsonIndex = 0;
        for (const auto& entry : data["entities"]) {
            const int thisIndex = jsonIndex++;

            if (delta && delta->DestroyedJsonIndices.count(thisIndex)) {
                continue; // destroyed in a previous session
            }

            std::string modelPath = entry["model"];
            auto pos = entry["position"];
            glm::vec3 finalPos(pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>());
            glm::quat finalRot(1.0f, 0.0f, 0.0f, 0.0f);

            if (delta) {
                auto movedIt = delta->MovedJsonEntities.find(thisIndex);
                if (movedIt != delta->MovedJsonEntities.end()) {
                    finalPos = movedIt->second.first;
                    finalRot = movedIt->second.second;
                }
            }

            auto model = GetOrLoadModel(modelPath);
            auto entity = scene.CreateEntity();
            auto& t = scene.Registry.get<Transform>(entity);
            t.Position = finalPos;
            t.Rotation = finalRot;
            scene.Registry.emplace<MeshRenderer>(entity, model);
            scene.Registry.emplace<ChunkId>(entity, ChunkId{ chunkX, chunkZ });
            scene.Registry.emplace<ChunkJsonIndex>(entity, ChunkJsonIndex{ thisIndex });

            count++;
        }

        Log::Info("Chunk ({}, {}) loaded: {} entities + ground plate + city block", chunkX, chunkZ, count);
    }

    // Runtime-spawned extras recorded for this chunk in a previous session
    if (delta) {
        for (const auto& spawned : delta->SpawnedEntities) {
            auto model = GetOrLoadModel(spawned.ModelPath);
            auto entity = scene.CreateEntity();
            auto& t = scene.Registry.get<Transform>(entity);
            t.Position = spawned.Position;
            t.Rotation = spawned.Rotation;
            scene.Registry.emplace<MeshRenderer>(entity, model);
            scene.Registry.emplace<ChunkId>(entity, ChunkId{ chunkX, chunkZ });
        }
    }
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
        // ground plate, and now buildings too) had their Jolt body silently
        // orphaned every unload.
        if (scene.Registry.all_of<RigidBody>(entity)) {
            const auto& rigidBody = scene.Registry.get<RigidBody>(entity);
            if (!rigidBody.BodyId.IsInvalid()) {
                physicsWorld.DestroyBody(rigidBody.BodyId);
            }
        }

        // NOTE: deliberately NOT calling RecordEntityDestructionDelta here.
        // Unloading is routine streaming (the chunk went out of range), not
        // a gameplay-driven destruction — recording a delta here would mark
        // every building/entity as permanently destroyed the moment you
        // walk away from it, since every chunk gets unloaded constantly as
        // the player moves. Deltas are recorded ONLY at the point of actual
        // destructive intent (see EditorUI::DeleteSelectedEntity, and any
        // future gameplay destruction system).
        scene.DestroyEntity(entity);
    }

    Log::Info("Chunk ({}, {}) unloaded: {} entities", chunkX, chunkZ, toDestroy.size());
}