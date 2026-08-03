#pragma once
#include "../scene/Scene.h"
#include "../core/AssetPaths.h"
#include <glm/glm.hpp>
#include <string>

// Rolling pedestrian population manager — replaces per-chunk baked
// pedestrians. Maintains a roughly-fixed number of active pedestrians in a
// bubble around the viewer, spawning new ones near the edge of that bubble
// and despawning ones that fall far behind, rather than tying pedestrian
// existence to chunk load/unload.
//
// This is what decouples "how far can I see" (ChunkManager's streaming
// radius, which can stay large for visual continuity) from "how many NPCs
// am I simulating" (this system's much smaller radius) — the separation
// that lets open-world games keep pedestrian density visually rich near
// the player without the total count scaling with how much world is
// currently loaded.
namespace PedestrianSpawnSystem {

struct Config {
int TargetPopulation = 40;
float SpawnRadius = 40.0f;
float DespawnRadius = 70.0f;
float ChunkSize = 50.0f;

// Matches SceneLoader.cpp's previous per-chunk pedestrian spawn — same
// placeholder player skeleton/clips, now loaded once and shared across
// every spawned pedestrian instead of being re-fetched per chunk.
std::string ModelPath = AssetPaths::Resolve(AssetPaths::Category::Models, "player/player.fbx");
std::string IdleAnimPath = AssetPaths::Resolve(AssetPaths::Category::Models, "player/Idle.fbx");
std::string WalkAnimPath = AssetPaths::Resolve(AssetPaths::Category::Models, "player/Walking.fbx");
glm::vec3 ModelScale{0.01f, 0.01f, 0.01f};

};

// Call once per frame; internally throttles the actual spawn/despawn work
// to run only a few times per second rather than every frame, since
// population bookkeeping doesn't need per-frame precision the way
// movement/animation does.
void Update(Scene& scene, const glm::vec3& viewerPosition, float deltaTime, const Config& config);

} // namespace PedestrianSpawnSystem