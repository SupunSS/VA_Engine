#pragma once
#include "Command.h"
#include "../scene/Scene.h"
#include "../scene/Components.h"
#include "../physics/PhysicsWorld.h"
#include "../rendering/Model.h"
#include "../rendering/Material.h"
#include <memory>
#include "../physics/VehicleController.h"
#include "../rendering/TerrainChunk.h"

// Concrete ICommand implementations for the editor's general-purpose
// undo/redo. Kept separate from Command.h itself (the generic mechanism)
// since these know about Scene/PhysicsWorld/Components specifically.

// Undoes/redoes a gizmo-driven Transform edit (move/rotate/scale), including
// re-syncing the matching physics body (RigidBody or vehicle chassis) the
// same way EditorUI::DrawTransformGizmo already does live during a drag —
// this class only needs to repeat that sync for the undo/redo case.
class TransformEditCommand : public ICommand {
public:
    TransformEditCommand(Scene& scene, PhysicsWorld& physicsWorld, entt::entity entity,
                          const Transform& before, const Transform& after)
        : m_scene(scene), m_physicsWorld(physicsWorld), m_entity(entity),
          m_before(before), m_after(after) {}

    void Do() override { Apply(m_after); }
    void Undo() override { Apply(m_before); }
    const char* GetLabel() const override { return "Move/Rotate/Scale"; }

private:
    void Apply(const Transform& target) {
        if (!m_scene.Registry.valid(m_entity) || !m_scene.Registry.all_of<Transform>(m_entity)) {
            return;
        }

        auto& transform = m_scene.Registry.get<Transform>(m_entity);
        transform.Position = target.Position;
        transform.Rotation = target.Rotation;
        transform.Scale = target.Scale;

        if (m_scene.Registry.all_of<RigidBody>(m_entity)) {
            auto& rigidBody = m_scene.Registry.get<RigidBody>(m_entity);
            if (!rigidBody.BodyId.IsInvalid()) {
                const JPH::RVec3 pos(transform.Position.x, transform.Position.y, transform.Position.z);
                const JPH::Quat rot(transform.Rotation.x, transform.Rotation.y, transform.Rotation.z, transform.Rotation.w);
                m_physicsWorld.GetBodyInterface().SetPositionAndRotation(rigidBody.BodyId, pos, rot, JPH::EActivation::Activate);
            }
        }

        if (m_scene.Registry.all_of<VehicleComponent>(m_entity)) {
            auto& vehicleComp = m_scene.Registry.get<VehicleComponent>(m_entity);
            if (vehicleComp.Controller) {
                const JPH::BodyID chassisBodyId = vehicleComp.Controller->GetBodyID();
                if (!chassisBodyId.IsInvalid()) {
                    const JPH::RVec3 pos(transform.Position.x, transform.Position.y, transform.Position.z);
                    const JPH::Quat rot(transform.Rotation.x, transform.Rotation.y, transform.Rotation.z, transform.Rotation.w);
                    m_physicsWorld.GetBodyInterface().SetPositionAndRotation(chassisBodyId, pos, rot, JPH::EActivation::Activate);
                }
            }
        }
    }

    Scene& m_scene;
    PhysicsWorld& m_physicsWorld;
    entt::entity m_entity;
    Transform m_before;
    Transform m_after;
};

// Undoes/redoes placing a model into the scene (Asset Browser drag-drop /
// double-click). Undo destroys the created entity; Redo recreates it
// identically. No physics body is created at placement time today, so
// none needs to be torn down/recreated here.
class CreateEntityCommand : public ICommand {
public:
    CreateEntityCommand(Scene& scene, std::shared_ptr<Model> model, const glm::vec3& position)
        : m_scene(scene), m_model(std::move(model)), m_position(position) {}

    void Do() override {
        m_entity = m_scene.CreateEntity();
        m_scene.Registry.get<Transform>(m_entity).Position = m_position;
        m_scene.Registry.emplace<MeshRenderer>(m_entity, m_model);
    }

    void Undo() override {
        if (m_entity != entt::null && m_scene.Registry.valid(m_entity)) {
            m_scene.DestroyEntity(m_entity);
        }
        m_entity = entt::null;
    }

    entt::entity GetEntity() const { return m_entity; }
    const char* GetLabel() const override { return "Place Object"; }

private:
    Scene& m_scene;
    std::shared_ptr<Model> m_model;
    glm::vec3 m_position;
    entt::entity m_entity = entt::null;
};

// Undoes/redoes deleting a scene entity. Snapshots the component types
// relevant to level-design-placed objects: Transform, MeshRenderer,
// RigidBody (box or sphere), ChunkId, BuildingPlot, ChunkJsonIndex.
//
// KNOWN LIMITATION: entity types with components beyond this list
// (VehicleComponent, AnimatorComponent, PedestrianAI, etc.) will lose
// those components on Undo — this command was scoped to what
// EditorUI::DeleteSelectedEntity is actually used for today (deleting
// placed props/buildings/physics-test bodies), not arbitrary entities.
//
// KNOWN LIMITATION: SceneLoader::RecordEntityDestructionDelta (called by
// the caller, not this class) persists the deletion into ChunkDeltaStore
// immediately on Do(). Undo() restores the entity visually for this
// session, but does NOT remove that persisted delta — if the entity's
// chunk streams out and back in after an undo, it will disappear again.
// A full fix needs a ChunkDeltaStore::RemoveDelta-style API.
class DeleteEntityCommand : public ICommand {
public:
    DeleteEntityCommand(Scene& scene, PhysicsWorld& physicsWorld, entt::entity entity)
        : m_scene(scene), m_physicsWorld(physicsWorld) {
        Snapshot(entity);
    }

    void Do() override {
        if (m_entity == entt::null || !m_scene.Registry.valid(m_entity)) return;

        if (m_hasRigidBody && m_scene.Registry.all_of<RigidBody>(m_entity)) {
            auto& rigidBody = m_scene.Registry.get<RigidBody>(m_entity);
            if (!rigidBody.BodyId.IsInvalid()) {
                m_physicsWorld.DestroyBody(rigidBody.BodyId);
            }
        }
        m_scene.DestroyEntity(m_entity);
        m_entity = entt::null;
    }

    void Undo() override {
        m_entity = m_scene.CreateEntity();
        auto& transform = m_scene.Registry.get<Transform>(m_entity);
        transform = m_transform;

        if (m_hasMeshRenderer) {
            m_scene.Registry.emplace<MeshRenderer>(m_entity, m_model, m_material);
        }
        if (m_hasChunkId) {
            m_scene.Registry.emplace<ChunkId>(m_entity, m_chunkId);
        }
        if (m_hasBuildingPlot) {
            m_scene.Registry.emplace<BuildingPlot>(m_entity, m_buildingPlot);
        }
        if (m_hasChunkJsonIndex) {
            m_scene.Registry.emplace<ChunkJsonIndex>(m_entity, m_chunkJsonIndex);
        }
        if (m_hasRigidBody) {
            JPH::BodyID bodyId;
            if (m_rigidBodyShape == PhysicsShapeType::Sphere) {
                bodyId = m_physicsWorld.CreateSphereBody(transform.Position, m_rigidBodySphereRadius, m_rigidBodyIsStatic);
            } else {
                bodyId = m_physicsWorld.CreateBoxBody(transform.Position, m_rigidBodyHalfExtents, m_rigidBodyIsStatic);
            }
            m_scene.Registry.emplace<RigidBody>(m_entity, bodyId, m_rigidBodyIsStatic, m_rigidBodyShape,
                                                 m_rigidBodyHalfExtents, m_rigidBodySphereRadius);
        }
    }

    const char* GetLabel() const override { return "Delete Object"; }

private:
    void Snapshot(entt::entity entity) {
        m_entity = entity;
        if (!m_scene.Registry.valid(entity)) return;

        m_transform = m_scene.Registry.get<Transform>(entity);

        if (m_scene.Registry.all_of<MeshRenderer>(entity)) {
            m_hasMeshRenderer = true;
            auto& renderer = m_scene.Registry.get<MeshRenderer>(entity);
            m_model = renderer.ModelRef;
            m_material = renderer.MaterialRef;
        }
        if (m_scene.Registry.all_of<ChunkId>(entity)) {
            m_hasChunkId = true;
            m_chunkId = m_scene.Registry.get<ChunkId>(entity);
        }
        if (m_scene.Registry.all_of<BuildingPlot>(entity)) {
            m_hasBuildingPlot = true;
            m_buildingPlot = m_scene.Registry.get<BuildingPlot>(entity);
        }
        if (m_scene.Registry.all_of<ChunkJsonIndex>(entity)) {
            m_hasChunkJsonIndex = true;
            m_chunkJsonIndex = m_scene.Registry.get<ChunkJsonIndex>(entity);
        }
        if (m_scene.Registry.all_of<RigidBody>(entity)) {
            m_hasRigidBody = true;
            auto& rigidBody = m_scene.Registry.get<RigidBody>(entity);
            m_rigidBodyIsStatic = rigidBody.IsStatic;
            m_rigidBodyShape = rigidBody.Shape;
            m_rigidBodyHalfExtents = rigidBody.BoxHalfExtents;
            m_rigidBodySphereRadius = rigidBody.SphereRadius;
        }
    }

    Scene& m_scene;
    PhysicsWorld& m_physicsWorld;
    entt::entity m_entity = entt::null;

    Transform m_transform;
    bool m_hasMeshRenderer = false;
    std::shared_ptr<Model> m_model;
    std::shared_ptr<Material> m_material;
    bool m_hasChunkId = false;
    ChunkId m_chunkId{};
    bool m_hasBuildingPlot = false;
    BuildingPlot m_buildingPlot{};
    bool m_hasChunkJsonIndex = false;
    ChunkJsonIndex m_chunkJsonIndex{};
    bool m_hasRigidBody = false;
    bool m_rigidBodyIsStatic = true;
    PhysicsShapeType m_rigidBodyShape = PhysicsShapeType::Box;
    glm::vec3 m_rigidBodyHalfExtents{0.5f};
    float m_rigidBodySphereRadius = 0.5f;
};

// Undoes/redoes one complete brush stroke (mouse-down to mouse-up), NOT
// one frame of dragging — the caller (EditorUI::UpdateTerrainSculpting)
// snapshots "before" once at stroke start and "after" once at stroke end,
// so one drag = one undo step, matching every other command in this file.
class TerrainStrokeCommand : public ICommand {
public:
    TerrainStrokeCommand(TerrainChunk& chunk, std::vector<float> before, std::vector<float> after)
        : m_chunk(chunk), m_before(std::move(before)), m_after(std::move(after)) {}

    void Do() override {
        m_chunk.SetHeights(m_after);
        m_chunk.RebuildMesh();
    }
    void Undo() override {
        m_chunk.SetHeights(m_before);
        m_chunk.RebuildMesh();
    }
    const char* GetLabel() const override { return "Sculpt Terrain"; }

private:
    // KNOWN LIMITATION: holds a raw reference to a TerrainChunk owned by
    // TerrainSystem's chunk map. If that chunk streams out (leaves
    // TerrainSystem's load radius) after the stroke but before an undo,
    // this reference dangles. Acceptable for Phase A (terrain load radius
    // is deliberately small during active sculpting); revisit if this
    // becomes a real crash in practice.
    TerrainChunk& m_chunk;
    std::vector<float> m_before;
    std::vector<float> m_after;
};