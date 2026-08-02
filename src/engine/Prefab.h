/**
 * Prefab System
 * 
 * Allows defining reusable entity templates in JSON format.
 * Developers can spawn fully configured entities without touching code.
 * 
 * Prefab file format (assets/prefabs/car_sport.json):
 * {
 *   "name": "car_sport",
 *   "components": {
 *     "Transform": { "position": [0, 1, 0], ... },
 *     "Model": { "meshPath": "models/car.gltf", ... },
 *     "Physics": { "mass": 1500, ... },
 *     "Vehicle": { "maxSpeed": 250, ... }
 *   }
 * }
 */

#pragma once

#include <engine/public/Types.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// NOTE: Deliberately NOT doing `using VAPublic::Transform;` (or similar) here.
// This header gets included alongside "../scene/Components.h" in some
// translation units (e.g. Engine.cpp via Adapters.h), which defines a
// *different*, real ECS `::Transform` component. A global-scope using
// declaration for VAPublic::Transform would collide with it and make
// `Transform` ambiguous wherever both are visible. Every VAPublic type is
// fully qualified below instead.

// Forward declarations
class Scene;

/**
 * Prefab definition loaded from JSON
 */
struct Prefab {
    std::string name;
    std::string description;
    json componentData;  // Raw component definitions
    json metadata;       // Additional metadata
};

/**
 * Prefab manager
 * 
 * Usage:
 *   PrefabManager& mgr = GetPrefabManager();
 *   mgr.LoadPrefab("assets/prefabs/car_sport.json");
 *   
 *   EntityId car = mgr.Instantiate("car_sport", position, &scene);
 */
class PrefabManager {
public:
    /**
     * Load a prefab from JSON file
     * @param filePath Path to prefab JSON file
     * @return Prefab name if successful
     */
    std::string LoadPrefab(const std::string& filePath);

    /**
     * Load all prefabs from a directory
     * @param directoryPath Path to directory containing .json prefabs
     * @return Number of prefabs loaded
     */
    int LoadPrefabsFromDirectory(const std::string& directoryPath);

    /**
     * Check if prefab exists
     * @param prefabName Name of prefab
     * @return true if loaded
     */
    bool HasPrefab(const std::string& prefabName) const;

    /**
     * Get prefab definition
     * @param prefabName Name of prefab
     * @return Pointer to prefab (nullptr if not found)
     */
    const Prefab* GetPrefab(const std::string& prefabName) const;

    /**
     * Instantiate an entity from a prefab
     * @param prefabName Name of prefab to spawn from
     * @param position Spawn position
     * @param scene Scene to add entity to (or nullptr to create only)
     * @return EntityId of new entity (0 if failed)
     */
    VAPublic::EntityId Instantiate(const std::string& prefabName, const VAPublic::Vec3& position, Scene* scene = nullptr);

    /**
     * Instantiate with custom position and rotation
     * @param prefabName Name of prefab
     * @param position Spawn position
     * @param rotation Spawn rotation
     * @param scene Scene to add entity to
     * @return EntityId of new entity
     */
    VAPublic::EntityId Instantiate(const std::string& prefabName, const VAPublic::Vec3& position,
                        const VAPublic::Quat& rotation, Scene* scene = nullptr);

    /**
     * Instantiate with full transform override
     * @param prefabName Name of prefab
     * @param transform Transform to apply
     * @param scene Scene to add entity to
     * @return EntityId of new entity
     */
    VAPublic::EntityId Instantiate(const std::string& prefabName, const VAPublic::Transform& transform,
                        Scene* scene = nullptr);

    /**
     * Clone an entity from another entity
     * @param sourceEntityId Entity to clone
     * @param position New position
     * @param scene Scene to add to
     * @return EntityId of clone
     */
    VAPublic::EntityId Clone(VAPublic::EntityId sourceEntityId, const VAPublic::Vec3& position, Scene* scene = nullptr);

    /**
     * Get all loaded prefab names
     * @return Vector of prefab names
     */
    std::vector<std::string> GetPrefabNames() const;

    /**
     * Unload a specific prefab
     * @param prefabName Name to unload
     */
    void UnloadPrefab(const std::string& prefabName);

    /**
     * Unload all prefabs
     */
    void UnloadAll();

    /**
     * Reload all prefabs from disk
     */
    void ReloadAll();

private:
    std::unordered_map<std::string, Prefab> m_prefabs;

    /**
     * Create entity from prefab data
     */
    VAPublic::EntityId CreateEntityFromPrefab(const Prefab& prefab, const VAPublic::Transform& transform, Scene* scene);

    /**
     * Apply component data from prefab
     */
    void ApplyComponent(VAPublic::EntityId entityId, const std::string& componentName,
                       const json& componentData, Scene* scene);
};

// ============================================================================
// GLOBAL PREFAB MANAGER
// ============================================================================

/**
 * Get the global prefab manager
 */
extern PrefabManager& GetPrefabManager();