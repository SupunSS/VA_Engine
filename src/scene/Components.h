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

    enum class State { Idle, Walk } CurrentState = State::Idle;
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