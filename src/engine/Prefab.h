#pragma once

#include <engine/public/Types.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// NOTE: Deliberately NOT doing `using VAPublic::Transform;` here.
// This header gets included alongside "../scene/Components.h" in some
// translation units (e.g. Engine.cpp via Adapters.h), which defines a
// *different*, real ECS `::Transform` component. A global-scope using
// declaration for VAPublic::Transform would collide with it and make
// `Transform` ambiguous wherever both are visible. Every VAPublic type is
// fully qualified below instead.

// Forward declarations — these are the real, concrete engine classes.
// Only pointers to them are needed here, so full definitions aren't required.
class Scene;
class PhysicsWorld;
class ScriptEngine;

/**
 * Prefab definition loaded from JSON
 */
struct Prefab {
    std::string name;
    std::string description;
    json componentData;  // Raw component definitions
    json metadata;        // Additional metadata
};

/**
 * Prefab manager
 *
 * Usage:
 *   PrefabManager& mgr = GetPrefabManager();
 *   mgr.LoadPrefab("assets/prefabs/car_sport.json");
 *
 *   EntityId car = mgr.Instantiate("car_sport", position, &scene, &physics, &scripting);
 *
 * physics and scripting are optional (default nullptr) — pass them only if
 * you want prefab "Physics"/"Vehicle" or "Script" component blocks to
 * actually take effect. Without them, those component types are skipped
 * with a log message instead of crashing.
 */
class PrefabManager {
public:
    std::string LoadPrefab(const std::string& filePath);
    int LoadPrefabsFromDirectory(const std::string& directoryPath);
    bool HasPrefab(const std::string& prefabName) const;
    const Prefab* GetPrefab(const std::string& prefabName) const;

    VAPublic::EntityId Instantiate(const std::string& prefabName, const VAPublic::Vec3& position,
                        Scene* scene = nullptr, PhysicsWorld* physics = nullptr, ScriptEngine* scripting = nullptr);

    VAPublic::EntityId Instantiate(const std::string& prefabName, const VAPublic::Vec3& position,
                        const VAPublic::Quat& rotation, Scene* scene = nullptr,
                        PhysicsWorld* physics = nullptr, ScriptEngine* scripting = nullptr);

    VAPublic::EntityId Instantiate(const std::string& prefabName, const VAPublic::Transform& transform,
                        Scene* scene = nullptr, PhysicsWorld* physics = nullptr, ScriptEngine* scripting = nullptr);

    VAPublic::EntityId Clone(VAPublic::EntityId sourceEntityId, const VAPublic::Vec3& position, Scene* scene = nullptr);

    std::vector<std::string> GetPrefabNames() const;
    void UnloadPrefab(const std::string& prefabName);
    void UnloadAll();
    void ReloadAll();

private:
    std::unordered_map<std::string, Prefab> m_prefabs;

    VAPublic::EntityId CreateEntityFromPrefab(const Prefab& prefab, const VAPublic::Transform& transform,
                        Scene* scene, PhysicsWorld* physics, ScriptEngine* scripting);

    void ApplyComponent(VAPublic::EntityId entityId, const std::string& componentName,
                       const json& componentData, Scene* scene, PhysicsWorld* physics, ScriptEngine* scripting);
};

extern PrefabManager& GetPrefabManager();