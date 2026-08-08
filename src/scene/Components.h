#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <entt/entt.hpp>
#include <memory>
#include <utility>
#include <vector>
#include <array>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include "../rendering/Material.h"
#include "../audio/AudioEngine.h"
#include <string>
#include "../rendering/AnimationStateMachine.h"

// Every entity that exists in the world has this. Cheap by design —
// static props (streetlights, trash cans) never touch anything beyond this.
struct Transform {
    glm::vec3 Position{0.0f};
    glm::quat Rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 Scale{1.0f};

    // Hierarchy support — parent is optional (entt::null if none).
    // Static-case-cheap: entities with no parent skip all hierarchy math.
    entt::entity Parent = entt::null;

    glm::mat4 GetLocalMatrix() const;
};

class Model;     // forward declare from rendering
class Texture;   // forward declare from rendering
class Animator;  // forward declare from rendering
class Animation; // forward declare from rendering

struct MeshRenderer {
    std::shared_ptr<Model> ModelRef;
    std::shared_ptr<Material> MaterialRef;

    MeshRenderer() = default;
    explicit MeshRenderer(std::shared_ptr<Model> model)
        : ModelRef(std::move(model)) {}
    MeshRenderer(std::shared_ptr<Model> model, std::shared_ptr<Material> material)
        : ModelRef(std::move(model)), MaterialRef(std::move(material)) {}
};

struct ChunkId {
    int x, z;
    bool operator==(const ChunkId& other) const { return x == other.x && z == other.z; }
};

class VehicleController;

struct PlayerTag {};

struct VehicleTag {};

struct VehicleOccupant {
    entt::entity DriverEntity = entt::null;
};

struct VehicleComponent {
    std::shared_ptr<VehicleController> Controller;
    std::array<entt::entity, 4> WheelEntities{ entt::null, entt::null, entt::null, entt::null };
};

// --- Pedestrian AI -----------------------------------------------------

struct PedestrianTag {};

// Drives a pedestrian's movement along a fixed waypoint loop (from
// CityLayout::GetSidewalkLoopWaypoints). Deliberately simple for this
// first stage — straight-line movement toward the next waypoint, no
// physics body, no obstacle avoidance yet.
struct PedestrianAI {
    std::vector<glm::vec3> PathWaypoints;
    int CurrentWaypointIndex = 0;
    float MoveSpeed = 1.4f;   // m/s, roughly an average human walking pace
    float WaitTimer = 0.0f;   // seconds remaining before departing the
                               // current waypoint — pausing briefly at each
                               // stop reads as far less robotic than
                               // instant direction changes every waypoint
};

// Wraps a per-entity Animator + the idle/walk clips it switches between.
// Every skinned entity that needs its own independent playback time (the
// player already does this via a standalone Animator in main.cpp — this
// component is the generalized, many-entities version of that same idea
// for NPCs).
struct AnimatorComponent {
    std::shared_ptr<Animator> AnimatorPtr;
    std::shared_ptr<Animation> IdleAnim;
    std::shared_ptr<Animation> WalkAnim;
    

    // Owns this entity's transition logic (Idle <-> Walk based on the
    // "IsMoving" parameter). Constructed once, lazily, the first time
    // PedestrianSystem needs it — see EnsureStateMachine() in
    // PedestrianSystem.cpp — since Components.h shouldn't need to know
    // how to wire up states/transitions itself.
    std::shared_ptr<AnimationStateMachine> StateMachine;
    std::shared_ptr<Model> SourceModel; // needed so PedestrianSystem can (re)load animation state machine JSON via LoadAnimation()
};

// --- HUD-backing data ----------------------------------------------------
// Real components with no gameplay system driving them yet (no damage
// source, no weapons) — but the HUD reads live data from these rather than
// hardcoded numbers, so nothing needs rewiring once combat/weapons exist.

struct Health {
    float Current = 100.0f;
    float Max = 100.0f;
};

struct Ammo {
    int Current = 0;
    int Reserve = 0;
};

// --- Physics test / generic rigid bodies --------------------------------

enum class PhysicsShapeType {
    Box = 0,
    Sphere = 1
};

struct PhysicsTestBody {};

// Attaches a Lua script to this entity, run in its own isolated
// environment (see ScriptEngine::AttachScript/CallEntityUpdates) — kept as
// just a path here, not the actual sol::environment, so this header stays
// free of sol types (Components.h is included almost everywhere).
struct ScriptComponent {
    std::string ScriptPath;
};

struct RigidBody {
    JPH::BodyID BodyId{};
    bool IsStatic = false;
    PhysicsShapeType Shape = PhysicsShapeType::Box;
    glm::vec3 BoxHalfExtents{0.5f};
    float SphereRadius = 0.5f;

    RigidBody() = default;
    RigidBody(
        JPH::BodyID bodyId,
        bool isStatic,
        PhysicsShapeType shape,
        const glm::vec3& boxHalfExtents,
        float sphereRadius
    )
        : BodyId(bodyId),
          IsStatic(isStatic),
          Shape(shape),
          BoxHalfExtents(boxHalfExtents),
          SphereRadius(sphereRadius) {}
};

// Generic looping/positional 3D audio source — ambient loops, or anything
// else that just needs to sit in the world and play. Position is synced
// from this entity's Transform every frame by AudioSystem; the component
// itself only stores tuning + the underlying engine handle.
struct AudioSource {
    AudioClipId Clip = kInvalidAudioClip;
    AudioSourceHandle Handle = kInvalidAudioSource;
    bool Loop = true;
    bool Autoplay = true;
    float Volume = 1.0f;
    float MinDistance = 2.0f;
    float MaxDistance = 50.0f;
};
 
// Lightweight, generic "is this entity currently moving" flag. Exists so
// FootstepAudio doesn't need to know about the player's character
// controller specifically — whatever system drives an entity's movement
// (main.cpp for the player) keeps this up to date, and AudioSystem just
// reads it. Pedestrians don't need this; they already expose movement via
// PedestrianAI::WaitTimer.
struct MovementState {
    bool IsMoving = false;
    bool IsRunning = false;
};
 
// Drives footstep one-shots for anything that walks (player or pedestrian).
// Deliberately separate from AudioSource since footsteps are timed
// one-shots, not a single persistent looping sound.
struct FootstepAudio {
    // Pool of footstep variations, picked randomly (no immediate repeats)
    // each time a step fires — see AudioSystem::PlayRandomFootstep. A
    // single-clip setup still works fine: just put one entry in the vector.
    std::vector<AudioClipId> WalkStepClips;
    std::vector<AudioClipId> RunStepClips; // optional — falls back to WalkStepClips if empty
    int LastPlayedIndex = -1;              // tracks which pool index played last, to avoid back-to-back repeats

    float StrideInterval = 0.42f;          // seconds between steps at walk pace — used ONLY as a fallback for entities with no Animator
    float RunStrideMultiplier = 0.65f;     // < 1 = faster steps while sprinting
    float StepTimer = 0.0f;
    float Volume = 0.6f;
    bool EventCallbackRegistered = false;
};

// Engine note for a vehicle: pitch/volume are driven every frame from the
// vehicle's current RPM (see AudioSystem::UpdateVehicleEngineSounds), on
// top of a persistent looping AudioSource created when the vehicle spawns.
struct VehicleEngineAudio {
    AudioClipId EngineLoopClip = kInvalidAudioClip;
    AudioSourceHandle Handle = kInvalidAudioSource;
    float MinPitch = 0.7f;
    float MaxPitch = 2.2f;
    float MinVolume = 0.35f;
    float MaxVolume = 1.0f;
    float ReferenceRpm = 6000.0f; // RPM that maps to MaxPitch/MaxVolume — tune to your redline
};

// Identifies a procedurally-generated building by its plot coordinate within
// the chunk (NOT array index — plot coords are stable across regenerations,
// since building layout is deterministically hashed from chunkX/chunkZ/plotSeed).
struct BuildingPlot {
    int PlotX, PlotZ;
};

// Identifies a hand-placed entity from a chunk's JSON file by its position
// in that file's "entities" array — stable as long as the JSON isn't reordered.
struct ChunkJsonIndex {
    int Index;
};