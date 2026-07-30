#include "PedestrianSpawnSystem.h"
#include "Components.h"
#include "CityLayout.h"
#include "SceneLoader.h"
#include "../rendering/Animator.h"
#include "../rendering/Model.h"
#include "../core/Log.h"
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <random>
#include <vector>

namespace PedestrianSpawnSystem {

namespace {
// Throttle: population bookkeeping runs a few times a second, not every
// frame — spawning/despawning a pedestrian a few hundred milliseconds
// "late" is completely imperceptible, and running this full scan every
// single frame would be wasted work.
constexpr float kUpdateIntervalSeconds = 0.5f;

std::mt19937& RngInstance() {
    static std::mt19937 rng{std::random_device{}()};
    return rng;
}

float RandomRange(float minVal, float maxVal) {
    std::uniform_real_distribution<float> dist(minVal, maxVal);
    return dist(RngInstance());
}

// Loaded once, shared across every pedestrian entity — matches the
// existing SceneLoader::GetOrLoadModel caching pattern, so spawning the
// 40th pedestrian doesn't reload the model/animation from disk.
struct PedestrianAssets {
    std::shared_ptr<Model> Model;
    std::shared_ptr<Animation> IdleAnim;
    std::shared_ptr<Animation> WalkAnim;
    bool Loaded = false;
};

PedestrianAssets& GetAssets(const Config& config) {
    static PedestrianAssets assets;
    if (!assets.Loaded) {
        assets.Model = SceneLoader::GetOrLoadModel(config.ModelPath);
        assets.IdleAnim = assets.Model->LoadAnimation(config.IdleAnimPath);
        assets.WalkAnim = assets.Model->LoadAnimation(config.WalkAnimPath);
        assets.Loaded = true;
    }
    return assets;
}

void SpawnOnePedestrian(Scene& scene, const glm::vec3& viewerPosition, const Config& config) {
    // Pick a random point in an annulus around the viewer (between half and
    // full SpawnRadius) rather than right at the edge — this avoids every
    // new pedestrian visibly popping in at the exact same distance ring,
    // and avoids spawning right on top of the player at 0 distance.
    const float angle = RandomRange(0.0f, glm::two_pi<float>());
    const float distance = RandomRange(config.SpawnRadius * 0.5f, config.SpawnRadius);
    const glm::vec3 spawnPos = viewerPosition + glm::vec3(std::cos(angle) * distance, 0.0f, std::sin(angle) * distance);

    const int chunkX = static_cast<int>(std::floor(spawnPos.x / config.ChunkSize));
    const int chunkZ = static_cast<int>(std::floor(spawnPos.z / config.ChunkSize));

    std::vector<glm::vec3> waypoints = CityLayout::GetSidewalkLoopWaypoints(chunkX, chunkZ, config.ChunkSize);
    if (waypoints.empty()) {
        return; // shouldn't happen given BuildInsetLoop always returns points, but guard anyway
    }

    // Start at whichever waypoint is closest to the chosen spawn point,
    // rather than always index 0 — keeps pedestrians from all "teleporting"
    // toward the same corner of their loop the instant they spawn.
    int closestIndex = 0;
    float closestDistSq = std::numeric_limits<float>::max();
    for (size_t i = 0; i < waypoints.size(); ++i) {
        const glm::vec3 diff = waypoints[i] - spawnPos;
        const float distSq = diff.x * diff.x + diff.z * diff.z;
        if (distSq < closestDistSq) {
            closestDistSq = distSq;
            closestIndex = static_cast<int>(i);
        }
    }

    auto& assets = GetAssets(config);

    auto entity = scene.CreateEntity();
    scene.Registry.emplace<PedestrianTag>(entity);

    auto& transform = scene.Registry.get<Transform>(entity);
    transform.Position = waypoints[closestIndex];
    transform.Scale = config.ModelScale;

    scene.Registry.emplace<MeshRenderer>(entity, assets.Model, nullptr);

    auto& ai = scene.Registry.emplace<PedestrianAI>(entity);
    ai.PathWaypoints = waypoints;
    ai.CurrentWaypointIndex = closestIndex;
    ai.MoveSpeed = 1.4f;
    ai.WaitTimer = 0.0f;

    auto& animComp = scene.Registry.emplace<AnimatorComponent>(entity);
    animComp.AnimatorPtr = std::make_shared<Animator>();
    animComp.IdleAnim = assets.IdleAnim;
    animComp.WalkAnim = assets.WalkAnim;
    animComp.CurrentState = AnimatorComponent::State::Idle;
    // Play idle immediately at spawn — without this, the pedestrian's
    // Animator has no active animation until PedestrianSystem::Update
    // first switches it, which is exactly what produced the T-pose you
    // saw: an entity that's rendered but has never had PlayAnimation
    // called yet is stuck at its raw bind pose.
    if (animComp.AnimatorPtr && animComp.IdleAnim) {
        animComp.AnimatorPtr->PlayAnimation(animComp.IdleAnim);
    }
}
}

void Update(Scene& scene, const glm::vec3& viewerPosition, float deltaTime, const Config& config) {
    static float accumulator = 0.0f;
    accumulator += deltaTime;
    if (accumulator < kUpdateIntervalSeconds) {
        return;
    }
    accumulator = 0.0f;

    // --- Despawn: remove pedestrians that fell far behind the viewer -----
    {
        std::vector<entt::entity> toDespawn;
        auto view = scene.Registry.view<Transform, PedestrianTag>();
        for (auto entity : view) {
            const auto& transform = view.get<Transform>(entity);
            const glm::vec3 diff = transform.Position - viewerPosition;
            const float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
            if (distSq > config.DespawnRadius * config.DespawnRadius) {
                toDespawn.push_back(entity);
            }
        }
        for (auto entity : toDespawn) {
            scene.DestroyEntity(entity);
        }
    }

    // --- Spawn: top up toward TargetPopulation ----------------------------
    const int currentCount = static_cast<int>(scene.Registry.view<PedestrianTag>().size());
    const int toSpawn = config.TargetPopulation - currentCount;
    for (int i = 0; i < toSpawn; ++i) {
        SpawnOnePedestrian(scene, viewerPosition, config);
    }

    Log::Info("[PedSpawn] current={} target={} spawned_this_tick={}", currentCount, config.TargetPopulation, toSpawn);
}

} // namespace PedestrianSpawnSystem