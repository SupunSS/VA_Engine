/**
 * Prefab System Implementation
 */

#include "Prefab.h"
#include "../scene/Scene.h"
#include "../scene/Components.h"
#include "../scene/SceneLoader.h"
#include "../physics/PhysicsWorld.h"
#include "../physics/VehicleController.h"
#include "../scripting/ScriptEngine.h"
#include <fstream>
#include <filesystem>
#include <iostream>

// NOTE: Deliberately NOT doing `using VAPublic::Transform;` here — this file
// includes "../scene/Components.h", which defines the real, different
// ::Transform ECS component. A using-declaration for VAPublic::Transform
// would collide with it. EntityId/Vec3/Quat don't have same-named global
// counterparts, so those are still safe to bring in unqualified.
using VAPublic::EntityId;
using VAPublic::Vec3;
using VAPublic::Quat;

namespace fs = std::filesystem;

namespace {

glm::vec3 ReadVec3(const json& data, const char* key, glm::vec3 defaultVal) {
    if (!data.contains(key)) return defaultVal;
    const json& arr = data.at(key);
    if (arr.is_array() && arr.size() >= 3) {
        return glm::vec3(arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>());
    }
    return defaultVal;
}

glm::quat ReadQuat(const json& data, const char* key, glm::quat defaultVal) {
    if (!data.contains(key)) return defaultVal;
    const json& arr = data.at(key);
    if (arr.is_array() && arr.size() >= 4) {
        return glm::quat(arr[3].get<float>(), arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>());
    }
    return defaultVal;
}

} // namespace

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

EntityId PrefabManager::Instantiate(const std::string& prefabName, const Vec3& position,
                                    Scene* scene, PhysicsWorld* physics, ScriptEngine* scripting) {
    Quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
    VAPublic::Transform transform{position, rotation, Vec3(1.0f, 1.0f, 1.0f)};
    return Instantiate(prefabName, transform, scene, physics, scripting);
}

EntityId PrefabManager::Instantiate(const std::string& prefabName, const Vec3& position,
                                    const Quat& rotation, Scene* scene,
                                    PhysicsWorld* physics, ScriptEngine* scripting) {
    VAPublic::Transform transform{position, rotation, Vec3(1.0f, 1.0f, 1.0f)};
    return Instantiate(prefabName, transform, scene, physics, scripting);
}

EntityId PrefabManager::Instantiate(const std::string& prefabName, const VAPublic::Transform& transform,
                                   Scene* scene, PhysicsWorld* physics, ScriptEngine* scripting) {
    const Prefab* prefab = GetPrefab(prefabName);
    if (!prefab) {
        std::cerr << "Prefab: '" << prefabName << "' not found" << std::endl;
        return 0;
    }
    return CreateEntityFromPrefab(*prefab, transform, scene, physics, scripting);
}

EntityId PrefabManager::Clone(EntityId sourceEntityId, const Vec3& position, Scene* scene) {
    // TODO: Implement entity cloning
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
    std::cout << "Prefab: Reloading all prefabs" << std::endl;
}

EntityId PrefabManager::CreateEntityFromPrefab(const Prefab& prefab, const VAPublic::Transform& transform,
                                               Scene* scene, PhysicsWorld* physics, ScriptEngine* scripting) {
    if (!scene) {
        return 0;
    }

    EntityId entityId = static_cast<EntityId>(scene->CreateEntity());
    auto entity = static_cast<entt::entity>(entityId);

    auto& realTransform = scene->Registry.get<::Transform>(entity);
    realTransform.Position = transform.position;
    realTransform.Rotation = transform.rotation;
    realTransform.Scale = transform.scale;

    for (const auto& [componentName, componentData] : prefab.componentData.items()) {
        ApplyComponent(entityId, componentName, componentData, scene, physics, scripting);
    }

    return entityId;
}

void PrefabManager::ApplyComponent(EntityId entityId, const std::string& componentName,
                                   const json& componentData, Scene* scene,
                                   PhysicsWorld* physics, ScriptEngine* scripting) {
    if (!scene) return;

    auto entity = static_cast<entt::entity>(entityId);
    auto& registry = scene->Registry;
    if (!registry.valid(entity)) {
        std::cerr << "Prefab: ApplyComponent called with invalid entity" << std::endl;
        return;
    }

    if (componentName == "Transform") {
        auto& realTransform = registry.get<::Transform>(entity);
        realTransform.Position = ReadVec3(componentData, "position", realTransform.Position);
        realTransform.Rotation = ReadQuat(componentData, "rotation", realTransform.Rotation);
        if (componentData.contains("scale")) {
            const json& scaleField = componentData.at("scale");
            if (scaleField.is_array()) {
                realTransform.Scale = ReadVec3(componentData, "scale", realTransform.Scale);
            } else if (scaleField.is_number()) {
                realTransform.Scale = glm::vec3(scaleField.get<float>());
            }
        }
        return;
    }

    if (componentName == "Model") {
        std::string meshPath = componentData.value("meshPath", "");
        if (meshPath.empty()) {
            std::cerr << "Prefab: Model component missing 'meshPath'" << std::endl;
            return;
        }
        auto model = SceneLoader::GetOrLoadModel(meshPath);
        if (!model) {
            std::cerr << "Prefab: Failed to load model '" << meshPath << "'" << std::endl;
            return;
        }
        auto material = std::make_shared<Material>();
        material->albedoTint = ReadVec3(componentData, "albedoTint", glm::vec3(1.0f));
        registry.emplace_or_replace<MeshRenderer>(entity, model, material);
        return;
    }

    if (componentName == "Health") {
        float max = componentData.value("max", 100.0f);
        float current = componentData.value("current", max);
        registry.emplace_or_replace<::Health>(entity, current, max);
        return;
    }

    if (componentName == "Ammo") {
        int current = componentData.value("current", 0);
        int reserve = componentData.value("reserve", 0);
        registry.emplace_or_replace<::Ammo>(entity, current, reserve);
        return;
    }

    if (componentName == "Audio") {
        std::string clipPath = componentData.value("clipPath", "");
        if (clipPath.empty()) {
            std::cerr << "Prefab: Audio component missing 'clipPath'" << std::endl;
            return;
        }
        AudioSource source;
        source.Clip = AudioEngine::Get().LoadClip(clipPath);
        source.Loop = componentData.value("loop", true);
        source.Autoplay = componentData.value("autoplay", true);
        source.Volume = componentData.value("volume", 1.0f);
        source.MinDistance = componentData.value("minDistance", 2.0f);
        source.MaxDistance = componentData.value("maxDistance", 50.0f);

        glm::vec3 spawnPos(0.0f);
        if (registry.all_of<::Transform>(entity)) {
            spawnPos = registry.get<::Transform>(entity).Position;
        }
        source.Handle = AudioEngine::Get().CreateSource3D(
            source.Clip, spawnPos, source.Loop, source.Autoplay,
            source.Volume, source.MinDistance, source.MaxDistance);
        registry.emplace_or_replace<AudioSource>(entity, source);
        return;
    }

    if (componentName == "Tag") {
        std::string tagName = componentData.is_string() ? componentData.get<std::string>()
                                                          : componentData.value("name", "");
        if (tagName == "Player") registry.emplace_or_replace<PlayerTag>(entity);
        else if (tagName == "Vehicle") registry.emplace_or_replace<VehicleTag>(entity);
        else if (tagName == "Pedestrian") registry.emplace_or_replace<PedestrianTag>(entity);
        else std::cerr << "Prefab: Unknown tag '" << tagName << "'" << std::endl;
        return;
    }

    if (componentName == "Physics") {
        if (!physics) {
            std::cerr << "Prefab: 'Physics' component present but no PhysicsWorld was provided — skipping" << std::endl;
            return;
        }

        glm::vec3 spawnPos(0.0f);
        if (registry.all_of<::Transform>(entity)) {
            spawnPos = registry.get<::Transform>(entity).Position;
        }

        bool isStatic = componentData.value("type", std::string("dynamic")) == "static";
        std::string shapeStr = componentData.value("shape", std::string("box"));

        JPH::BodyID bodyId;
        PhysicsShapeType shapeType;
        glm::vec3 boxHalfExtents(0.5f);
        float sphereRadius = 0.5f;

        if (shapeStr == "sphere") {
            sphereRadius = componentData.value("radius", 0.5f);
            bodyId = physics->CreateSphereBody(spawnPos, sphereRadius, isStatic);
            shapeType = PhysicsShapeType::Sphere;
        } else {
            boxHalfExtents = ReadVec3(componentData, "halfExtents", glm::vec3(0.5f));
            bodyId = physics->CreateBoxBody(spawnPos, boxHalfExtents, isStatic);
            shapeType = PhysicsShapeType::Box;
        }

        registry.emplace_or_replace<::RigidBody>(entity, bodyId, isStatic, shapeType, boxHalfExtents, sphereRadius);
        return;
    }

    if (componentName == "Vehicle") {
        if (!physics) {
            std::cerr << "Prefab: 'Vehicle' component present but no PhysicsWorld was provided — skipping" << std::endl;
            return;
        }

        glm::vec3 spawnPos(0.0f);
        if (registry.all_of<::Transform>(entity)) {
            spawnPos = registry.get<::Transform>(entity).Position;
        }

        auto controller = std::make_shared<VehicleController>(*physics, spawnPos);
        if (componentData.contains("maxEngineTorque")) controller->SetEngineTorque(componentData.value("maxEngineTorque", 600.0f));
        if (componentData.contains("tireFriction")) controller->SetTireFriction(componentData.value("tireFriction", 1.8f));
        if (componentData.contains("maxSteerAngleDegrees")) controller->SetMaxSteerAngleDegrees(componentData.value("maxSteerAngleDegrees", 35.0f));
        if (componentData.contains("suspensionFrequency") || componentData.contains("suspensionDamping")) {
            controller->SetSuspensionParameters(
                componentData.value("suspensionFrequency", 2.2f),
                componentData.value("suspensionDamping", 0.85f));
        }

        registry.emplace_or_replace<VehicleTag>(entity);
        registry.emplace_or_replace<VehicleOccupant>(entity);
        auto& vehicleComp = registry.emplace_or_replace<VehicleComponent>(entity);
        vehicleComp.Controller = controller;

        // NOTE: this does not create the 4 visual wheel entities (with
        // MeshRenderer) that main.cpp's SpawnTestVehicle() creates by hand —
        // that's rendering-specific setup outside this file's scope. See
        // SpawnTestVehicle() in main.cpp for the reference pattern if you
        // want prefab-spawned vehicles to also get visual wheel meshes.
        return;
    }

    if (componentName == "Script") {
        if (!scripting) {
            std::cerr << "Prefab: 'Script' component present but no ScriptEngine was provided — skipping" << std::endl;
            return;
        }

        std::string scriptPath = componentData.value("mainScript", componentData.value("scriptPath", std::string("")));
        if (scriptPath.empty()) {
            std::cerr << "Prefab: Script component missing 'mainScript'/'scriptPath'" << std::endl;
            return;
        }

        // NOTE: ScriptEngine's current API (see ScriptEngine.h) only
        // supports running a script globally against the whole scene —
        // there is no per-entity script context or entity-id binding yet.
        // This runs the script now; it does not scope execution to this
        // specific entity. If per-entity scripting is needed later, that's
        // a ScriptEngine API addition, not something fixable from here.
        scripting->RunScript(scriptPath);
        return;
    }

    std::cerr << "Prefab: Unknown component type '" << componentName << "'" << std::endl;
}

static PrefabManager g_prefabManager;

PrefabManager& GetPrefabManager() {
    return g_prefabManager;
}