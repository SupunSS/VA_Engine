/**
 * Prefab System Implementation
 */

#include "Prefab.h"
#include "../scene/Scene.h"
#include <fstream>
#include <filesystem>
#include <iostream>

// Safe here (unlike in Prefab.h): this translation unit never includes
// "../scene/Components.h", so there's no real ::Transform to collide with.
using VAPublic::EntityId;
using VAPublic::Vec3;
using VAPublic::Quat;
using VAPublic::Transform;

namespace fs = std::filesystem;

std::string PrefabManager::LoadPrefab(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "Prefab: Failed to open " << filePath << std::endl;
            return "";
        }

        json data;
        file >> data;

        Prefab prefab;
        prefab.name = data.value("name", "");
        prefab.description = data.value("description", "");
        prefab.componentData = data.value("components", json::object());
        prefab.metadata = data.value("metadata", json::object());

        if (prefab.name.empty()) {
            std::cerr << "Prefab: Missing 'name' field in " << filePath << std::endl;
            return "";
        }

        m_prefabs[prefab.name] = prefab;
        std::cout << "Prefab: Loaded '" << prefab.name << "'" << std::endl;

        return prefab.name;
    }
    catch (const std::exception& e) {
        std::cerr << "Prefab: Error loading " << filePath << ": " << e.what() << std::endl;
        return "";
    }
}

int PrefabManager::LoadPrefabsFromDirectory(const std::string& directoryPath) {
    int count = 0;

    try {
        for (const auto& entry : fs::directory_iterator(directoryPath)) {
            if (entry.path().extension() == ".json") {
                if (!LoadPrefab(entry.path().string()).empty()) {
                    count++;
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Prefab: Error loading directory " << directoryPath << ": " << e.what() << std::endl;
    }

    return count;
}

bool PrefabManager::HasPrefab(const std::string& prefabName) const {
    return m_prefabs.find(prefabName) != m_prefabs.end();
}

const Prefab* PrefabManager::GetPrefab(const std::string& prefabName) const {
    auto it = m_prefabs.find(prefabName);
    if (it != m_prefabs.end()) {
        return &it->second;
    }
    return nullptr;
}

EntityId PrefabManager::Instantiate(const std::string& prefabName, const Vec3& position, Scene* scene) {
    Quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
    Transform transform{position, rotation, Vec3(1.0f, 1.0f, 1.0f)};
    return Instantiate(prefabName, transform, scene);
}

EntityId PrefabManager::Instantiate(const std::string& prefabName, const Vec3& position,
                                    const Quat& rotation, Scene* scene) {
    Transform transform{position, rotation, Vec3(1.0f, 1.0f, 1.0f)};
    return Instantiate(prefabName, transform, scene);
}

EntityId PrefabManager::Instantiate(const std::string& prefabName, const Transform& transform,
                                   Scene* scene) {
    const Prefab* prefab = GetPrefab(prefabName);
    if (!prefab) {
        std::cerr << "Prefab: '" << prefabName << "' not found" << std::endl;
        return 0;
    }

    return CreateEntityFromPrefab(*prefab, transform, scene);
}

EntityId PrefabManager::Clone(EntityId sourceEntityId, const Vec3& position, Scene* scene) {
    // TODO: Implement entity cloning
    // This would copy all components from source entity and create new entity
    return 0;
}

std::vector<std::string> PrefabManager::GetPrefabNames() const {
    std::vector<std::string> names;
    for (const auto& [name, prefab] : m_prefabs) {
        names.push_back(name);
    }
    return names;
}

void PrefabManager::UnloadPrefab(const std::string& prefabName) {
    auto it = m_prefabs.find(prefabName);
    if (it != m_prefabs.end()) {
        m_prefabs.erase(it);
        std::cout << "Prefab: Unloaded '" << prefabName << "'" << std::endl;
    }
}

void PrefabManager::UnloadAll() {
    m_prefabs.clear();
    std::cout << "Prefab: Unloaded all prefabs" << std::endl;
}

void PrefabManager::ReloadAll() {
    // TODO: Store original paths and reload from disk
    std::cout << "Prefab: Reloading all prefabs" << std::endl;
}

EntityId PrefabManager::CreateEntityFromPrefab(const Prefab& prefab, const Transform& transform, Scene* scene) {
    // TODO: Implement entity creation from prefab
    // This would:
    // 1. Create entity in scene (if provided)
    // 2. Apply Transform component
    // 3. Apply all other components based on componentData
    // 4. Handle component initialization (models, physics, scripts, etc.)
    
    if (scene) {
        EntityId entityId = static_cast<EntityId>(scene->CreateEntity());
        
        // Apply all components from prefab
        for (const auto& [componentName, componentData] : prefab.componentData.items()) {
            ApplyComponent(entityId, componentName, componentData, scene);
        }
        
        return entityId;
    }

    return 0;
}

void PrefabManager::ApplyComponent(EntityId entityId, const std::string& componentName,
                                   const json& componentData, Scene* scene) {
    // TODO: Component application logic
    // This would:
    // - Parse component type from name
    // - Create appropriate component
    // - Apply serialized data to component
    // 
    // Example components:
    // - Transform: position, rotation, scale
    // - Model: meshPath, materialPath
    // - Physics: mass, drag, type
    // - Vehicle: speed, handling, etc.
    // - Script: scriptPath, scriptData
    // - Audio: soundPath, volume, loop

    std::cout << "Prefab: Applying component '" << componentName << "' to entity " << entityId << std::endl;
}

// ============================================================================
// GLOBAL PREFAB MANAGER
// ============================================================================

static PrefabManager g_prefabManager;

PrefabManager& GetPrefabManager() {
    return g_prefabManager;
}