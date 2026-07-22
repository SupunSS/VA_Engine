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

std::shared_ptr<Material> GetGroundMaterial() {
    static std::shared_ptr<Material> groundMaterial = [] {
        auto material = std::make_shared<Material>();
        material->albedoTint = glm::vec3(0.5f, 0.5f, 0.5f);
        return material;
    }();
    return groundMaterial;
}

// ---------------------------------------------------------------------------
// Procedural city block generation. Each chunk is treated as one city
// block: a road+sidewalk ring around the edge (lines up seamlessly with the
// next chunk's ring, since every chunk uses the same margin), with a small
// grid of block-out buildings filling the interior. Fully deterministic per
// (chunkX, chunkZ) — reloading the same chunk always regenerates identical
// geometry, so nothing drifts across an unload/reload cycle.
// ---------------------------------------------------------------------------
constexpr float kRoadWidth = 8.0f;
constexpr float kSidewalkWidth = 2.0f;

// Road/sidewalk surfaces are embedded slightly below y=0 and poke up just
// slightly above it — guarantees no floating gap above the ground plate's
// top (which sits exactly at y=0) and no z-fighting with it, without
// needing razor-thin boxes.
constexpr float kRoadTopY = 0.05f;
constexpr float kRoadBottomY = -0.15f;
constexpr float kSidewalkTopY = 0.15f; // sits above the road like a real curb
constexpr float kSidewalkBottomY = -0.15f;

constexpr int   kBuildingGridSize = 2; // buildings per chunk edge (2x2 = 4 buildings/chunk)
constexpr float kBuildingMargin = 1.0f; // gap between a building's footprint and its plot edge
constexpr float kBuildingMinHeight = 6.0f;
constexpr float kBuildingMaxHeight = 24.0f;

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
    scene.Registry.emplace<MeshRenderer>(entity, SceneLoader::GetOrLoadModel("models/cube.obj"), std::move(material));
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
    const float worldX0 = chunkX * chunkSize;
    const float worldZ0 = chunkZ * chunkSize;
    const float worldXCenter = worldX0 + chunkSize * 0.5f;
    const float worldZCenter = worldZ0 + chunkSize * 0.5f;

    // --- Road ring: 4 strips along the chunk edges. Corners double-cover —
    // harmless, same flat color, coplanar. ------------------------------
    CreateSurfaceBox(scene, worldXCenter, worldZ0 + kRoadWidth * 0.5f,
                      chunkSize * 0.5f, kRoadWidth * 0.5f, kRoadTopY, kRoadBottomY, GetRoadMaterial(), chunkX, chunkZ);
    CreateSurfaceBox(scene, worldXCenter, worldZ0 + chunkSize - kRoadWidth * 0.5f,
                      chunkSize * 0.5f, kRoadWidth * 0.5f, kRoadTopY, kRoadBottomY, GetRoadMaterial(), chunkX, chunkZ);
    CreateSurfaceBox(scene, worldX0 + kRoadWidth * 0.5f, worldZCenter,
                      kRoadWidth * 0.5f, chunkSize * 0.5f, kRoadTopY, kRoadBottomY, GetRoadMaterial(), chunkX, chunkZ);
    CreateSurfaceBox(scene, worldX0 + chunkSize - kRoadWidth * 0.5f, worldZCenter,
                      kRoadWidth * 0.5f, chunkSize * 0.5f, kRoadTopY, kRoadBottomY, GetRoadMaterial(), chunkX, chunkZ);

    // --- Sidewalk ring: just inside the road ring -----------------------
    const float sidewalkInset = kRoadWidth;
    const float sidewalkSpan = chunkSize - 2.0f * sidewalkInset;
    CreateSurfaceBox(scene, worldXCenter, worldZ0 + sidewalkInset + kSidewalkWidth * 0.5f,
                      sidewalkSpan * 0.5f, kSidewalkWidth * 0.5f, kSidewalkTopY, kSidewalkBottomY, GetSidewalkMaterial(), chunkX, chunkZ);
    CreateSurfaceBox(scene, worldXCenter, worldZ0 + chunkSize - sidewalkInset - kSidewalkWidth * 0.5f,
                      sidewalkSpan * 0.5f, kSidewalkWidth * 0.5f, kSidewalkTopY, kSidewalkBottomY, GetSidewalkMaterial(), chunkX, chunkZ);
    CreateSurfaceBox(scene, worldX0 + sidewalkInset + kSidewalkWidth * 0.5f, worldZCenter,
                      kSidewalkWidth * 0.5f, sidewalkSpan * 0.5f, kSidewalkTopY, kSidewalkBottomY, GetSidewalkMaterial(), chunkX, chunkZ);
    CreateSurfaceBox(scene, worldX0 + chunkSize - sidewalkInset - kSidewalkWidth * 0.5f, worldZCenter,
                      kSidewalkWidth * 0.5f, sidewalkSpan * 0.5f, kSidewalkTopY, kSidewalkBottomY, GetSidewalkMaterial(), chunkX, chunkZ);

    // --- Buildings: block-out boxes, real physics colliders --------------
    const float interiorMargin = kRoadWidth + kSidewalkWidth;
    const float interiorSpan = chunkSize - 2.0f * interiorMargin;
    const float plotSize = interiorSpan / static_cast<float>(kBuildingGridSize);

    for (int plotX = 0; plotX < kBuildingGridSize; ++plotX) {
        for (int plotZ = 0; plotZ < kBuildingGridSize; ++plotZ) {
            const float plotCenterX = worldX0 + interiorMargin + plotSize * (plotX + 0.5f);
            const float plotCenterZ = worldZ0 + interiorMargin + plotSize * (plotZ + 0.5f);

            const float footprintHalf = (plotSize * 0.5f) - kBuildingMargin;
            if (footprintHalf <= 0.5f) {
                continue; // margins too large for this plot size — skip rather than produce a degenerate building
            }

            const int plotSeed = plotX * 1000 + plotZ;
            const float heightT = HashToUnitFloat(chunkX, chunkZ, plotSeed);
            const float buildingHeight = kBuildingMinHeight + heightT * (kBuildingMaxHeight - kBuildingMinHeight);
            const uint32_t colorBucket = HashCoords(chunkX, chunkZ, plotSeed + 7919); // different salt so height/color don't correlate

            const glm::vec3 halfExtents(footprintHalf, buildingHeight * 0.5f, footprintHalf);
            const glm::vec3 center(plotCenterX, buildingHeight * 0.5f, plotCenterZ);

            auto entity = scene.CreateEntity();
            auto& transform = scene.Registry.get<Transform>(entity);
            transform.Position = center;
            transform.Scale = halfExtents;

            scene.Registry.emplace<MeshRenderer>(entity, SceneLoader::GetOrLoadModel("models/cube.obj"), GetBuildingMaterial(colorBucket));

            const auto bodyId = physicsWorld.CreateBoxBody(center, halfExtents, /*isStatic=*/true);
            scene.Registry.emplace<RigidBody>(entity, bodyId, true, PhysicsShapeType::Box, halfExtents, 0.5f);
            scene.Registry.emplace<ChunkId>(entity, ChunkId{ chunkX, chunkZ });
        }
    }
}
} // namespace

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
        groundTransform.Scale = halfExtents;

        auto groundModel = GetOrLoadModel("models/cube.obj");
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

    std::ifstream file(path);
    if (!file.is_open()) {
        Log::Warn("Chunk file not found: {} (ground plate + city block still created)", path);
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

    Log::Info("Chunk ({}, {}) loaded: {} entities + ground plate + city block", chunkX, chunkZ, count);
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
        scene.DestroyEntity(entity);
    }

    Log::Info("Chunk ({}, {}) unloaded: {} entities", chunkX, chunkZ, toDestroy.size());
}