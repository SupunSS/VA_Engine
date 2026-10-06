#pragma once

#include "Types.h"
#include "EventSystem.h"
#include <string>
#include <memory>

namespace VAPublic {

// Forward declarations — now correctly inside VAPublic, so these refer
// to VAPublic::IScene, VAPublic::IPhysicsWorld, etc. (previously these
// silently forward-declared a second, unrelated set of global-namespace
// classes with the same names — a bug in its own right).
class IScene;
class IPhysicsWorld;
class IRenderSystem;
class IAudioEngine;
class IScriptingEngine;

// ============================================================================
// ENGINE INTERFACE
// ============================================================================

class IEngine {
public:
    virtual ~IEngine() = default;

    // SCENE MANAGEMENT
    virtual void LoadScene(const std::string& sceneFilePath) = 0;
    virtual void SaveScene(const std::string& sceneFilePath) = 0;
    virtual IScene& GetScene() = 0;
    virtual entt::registry& GetRegistry() = 0;

    // ENTITY MANAGEMENT
    virtual EntityId SpawnEntity(const std::string& prefabName, const Vec3& position) = 0;
    virtual void DestroyEntity(EntityId entityId) = 0;
    virtual bool IsEntityValid(EntityId entityId) const = 0;

    // TRANSFORM QUERIES
    virtual void SetPosition(EntityId entityId, const Vec3& position) = 0;
    virtual Vec3 GetPosition(EntityId entityId) const = 0;
    virtual void SetRotation(EntityId entityId, const Quat& rotation) = 0;
    virtual Quat GetRotation(EntityId entityId) const = 0;

    // PHYSICS
    virtual IPhysicsWorld& GetPhysics() = 0;
    virtual RaycastHit Raycast(const Vec3& origin, const Vec3& direction, float maxDistance = 1000.0f) = 0;
    virtual void ApplyImpulse(EntityId entityId, const Vec3& impulse) = 0;

    // AUDIO
    virtual IAudioEngine& GetAudio() = 0;
    virtual void PlaySound(const std::string& soundPath, const Vec3* position = nullptr,
                          float volume = 1.0f, bool loop = false) = 0;

    // INPUT
    virtual InputState GetInput() const = 0;
    virtual bool IsKeyHeld(KeyCode key) const = 0;
    virtual void RemapInput(const std::string& actionName, KeyCode key) = 0;

    // SCRIPTING
    virtual IScriptingEngine& GetScripting() = 0;
    virtual void ExecuteScript(const std::string& scriptPath, const std::string& scriptName = "") = 0;
    virtual void ReloadScript(const std::string& scriptName) = 0;

    // RENDERING
    virtual IRenderSystem& GetRender() = 0;
    virtual Vec3 GetCameraPosition() const = 0;
    virtual void SetCameraPosition(const Vec3& position) = 0;

    // EVENTS
    virtual IEventSystem& GetEvents() = 0;

    // DEBUG
    virtual bool IsDeveloperMode() const = 0;
    virtual void Log(const std::string& message) = 0;
    virtual void SetDebugRendering(bool enabled) = 0;

    // LIFECYCLE
    virtual bool ShouldExit() const = 0;
    virtual void RequestExit() = 0;
    virtual float GetDeltaTime() const = 0;
    virtual float GetElapsedTime() const = 0;
};

// ============================================================================
// ENGINE INITIALIZATION
// ============================================================================

extern IEngine* GetEngine();
extern IEngine* InitializeEngine(const std::string& configPath = "config/engine.yaml");
extern void ShutdownEngine();

extern void ConnectEngineSystems(IEngine* engine, void* scene, void* physicsWorld,
                                 void* audioEngine, void* scriptEngine, void* camera);

} // namespace VAPublic