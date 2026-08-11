#pragma once
#include "../scene/Scene.h"
#include "../core/AssetPaths.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

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

// One pedestrian "type" — a state machine archetype paired with the move
// speed that actually matches its Walk clip's stride pace. Kept together
// deliberately: a fast clip (e.g. Running.fbx) at a slow MoveSpeed reads
// as feet sliding/overstepping, so these two values must never drift
// apart by picking one independently of the other.
struct PedestrianArchetype {
    std::string StateMachinePath;
    float MoveSpeed = 1.4f;
};

struct Config {
    int TargetPopulation = 40;
    float SpawnRadius = 40.0f;
    float DespawnRadius = 70.0f;
    float ChunkSize = 50.0f;

    // Matches SceneLoader.cpp's previous per-chunk pedestrian spawn — same
    // placeholder player skeleton, now loaded once and shared across every
    // spawned pedestrian instead of being re-fetched per chunk.
    std::string ModelPath = AssetPaths::Resolve(AssetPaths::Category::Models, "player/player.fbx");

    // Fallback pose only — played immediately at spawn so the entity isn't
    // stuck at its raw bind pose (T-pose) before PedestrianSystem gets a
    // chance to assign a real state machine state. Actual idle/walk/etc.
    // clips for gameplay come from whichever archetype in Archetypes this
    // pedestrian was randomly assigned.
    std::string IdleAnimPath = AssetPaths::Resolve(AssetPaths::Category::Models, "player/Idle.fbx");

    glm::vec3 ModelScale{0.01f, 0.01f, 0.01f};

    // One archetype is chosen at random per spawned pedestrian — this is
    // what gives different pedestrians genuinely different clips,
    // transitions, AND matching movement speed, rather than every
    // instance sharing one machine. Must be populated by the caller
    // (main.cpp) before pedestrians start spawning; a pedestrian spawned
    // while this is empty gets no animation transitions at all (see the
    // warning logged in SpawnOnePedestrian).
    std::vector<PedestrianArchetype> Archetypes;
};

// Call once per frame; internally throttles the actual spawn/despawn work
// to run only a few times per second rather than every frame, since
// population bookkeeping doesn't need per-frame precision the way
// movement/animation does.
void Update(Scene& scene, const glm::vec3& viewerPosition, float deltaTime, const Config& config);

} // namespace PedestrianSpawnSystem