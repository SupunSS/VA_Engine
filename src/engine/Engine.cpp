/**
 * Engine Implementation
 * 
 * This file implements the IEngine interface and integrates all engine systems.
 * It bridges the public API with the existing engine implementation in /src/.
 */

#include <engine/public/EngineAPI.h>
#include <engine/public/EventSystem.h>
#include <engine/Config.h>
#include <engine/Prefab.h>
#include <engine/HotReload.h>
#include <iostream>
#include <stdexcept>

// Forward declare real engine systems from /src/ (we use void* to avoid circular includes)
// Actual implementations are in:
// - ::Scene (scene/Scene.h)
// - ::PhysicsWorld (physics/PhysicsWorld.h)
// - ::AudioEngine (audio/AudioEngine.h)
// - ::ScriptEngine (scripting/ScriptEngine.h)
// - ::Camera (rendering/Camera.h)

// Forward declare internal systems
extern IEventSystem& GetEventSystem();
extern void ShutdownEventSystem();

/**
 * Dummy implementations of abstract classes for API compatibility
 */
class DummyScene : public ::Scene {
public:
    EntityId CreateEntity() override { return 0; }
    void DestroyEntity(EntityId) override {}
    EntityId FindEntityByName(const std::string&) override { return 0; }
    uint32_t GetEntityCount() const override { return 0; }
    void SetSimulationSpeed(float) override {}
    float GetSimulationSpeed() const override { return 1.0f; }
    void Pause() override {}
    void Resume() override {}
    bool IsPaused() const override { return false; }
};

class DummyPhysicsWorld : public ::PhysicsWorld {
public:
    std::vector<RaycastHit> RaycastAll(const Vec3&, const Vec3&, float) override { return {}; }
    RaycastHit Raycast(const Vec3&, const Vec3&, float) override { return RaycastHit{}; }
    std::vector<EntityId> QuerySphere(const Vec3&, float) override { return {}; }
    std::vector<EntityId> QueryBox(const Vec3&, const Vec3&) override { return {}; }
    void SetGravity(const Vec3&) override {}
    Vec3 GetGravity() const override { return Vec3(0, -9.81f, 0); }
    void Update(float) override {}
};

class DummyAudioEngine : public ::AudioEngine {
public:
    uint32_t LoadClip(const std::string&) override { return 0; }
    void UnloadClip(uint32_t) override {}
    AudioSourceId PlayClip(uint32_t, const Vec3* = nullptr, float = 1.0f, float = 1.0f, bool = false) override { return 0; }
    void Stop(AudioSourceId) override {}
    void Pause(AudioSourceId) override {}
    void Resume(AudioSourceId) override {}
    bool IsPlaying(AudioSourceId) override { return false; }
    void SetVolume(AudioSourceId, float) override {}
    void SetPosition(AudioSourceId, const Vec3&) override {}
    void SetListenerTransform(const Vec3&, const Vec3&, const Vec3&) override {}
    void Update(float) override {}
};

class DummyScriptingEngine : public ::ScriptingEngine {
public:
    void ExecuteScript(const std::string&, const std::string& = "") override {}
    void ExecuteCode(const std::string&) override {}
    void ReloadScript(const std::string&) override {}
    void SetGlobal(const std::string&, const void*) override {}
    sol::state* GetLuaState() override { return nullptr; }
    void CallFunction(const std::string&) override {}
    bool HasFunction(const std::string&) override { return false; }
};

class DummyRenderSystem : public ::RenderSystem {
public:
    Vec3 GetCameraPosition() const override { return Vec3(0, 0, 0); }
    void SetCameraPosition(const Vec3&) override {}
    Vec3 GetCameraForward() const override { return Vec3(0, 0, -1); }
    Vec3 GetCameraRight() const override { return Vec3(1, 0, 0); }
    Vec3 GetCameraUp() const override { return Vec3(0, 1, 0); }
    void SetCameraLookAt(const Vec3&, const Vec3&, const Vec3&) override {}
    void SetDebugRendering(bool) override {}
    bool IsDebugRenderingEnabled() const override { return false; }
    int GetViewportWidth() const override { return 1920; }
    int GetViewportHeight() const override { return 1080; }
    uint32_t LoadTexture(const std::string&) override { return 0; }
    uint32_t LoadModel(const std::string&) override { return 0; }
};

/**
 * Main Engine class - implements IEngine interface
 */
class EngineImpl : public IEngine {
public:
    EngineImpl();
    ~EngineImpl() override;

    // Initialization
    bool Initialize(const std::string& configPath);
    void Shutdown();

    // Set pointers to real systems (called from existing engine)
    void SetScene(void* scene) { m_realScene = scene; }
    void SetPhysicsWorld(void* physics) { m_realPhysics = physics; }
    void SetAudioEngine(void* audio) { m_realAudio = audio; }
    void SetScriptEngine(void* script) { m_realScript = script; }
    void SetCamera(void* camera) { m_camera = camera; }

    // ====== Scene Management ======
    void LoadScene(const std::string& sceneFilePath) override;
    void SaveScene(const std::string& sceneFilePath) override;
    Scene& GetScene() override;
    entt::registry& GetRegistry() override;

    // ====== Entity Management ======
    EntityId SpawnEntity(const std::string& prefabName, const Vec3& position) override;
    void DestroyEntity(EntityId entityId) override;
    bool IsEntityValid(EntityId entityId) const override;

    // ====== Transform Queries ======
    void SetPosition(EntityId entityId, const Vec3& position) override;
    Vec3 GetPosition(EntityId entityId) const override;
    void SetRotation(EntityId entityId, const Quat& rotation) override;
    Quat GetRotation(EntityId entityId) const override;

    // ====== Physics ======
    PhysicsWorld& GetPhysics() override;
    RaycastHit Raycast(const Vec3& origin, const Vec3& direction, float maxDistance = 1000.0f) override;
    void ApplyImpulse(EntityId entityId, const Vec3& impulse) override;

    // ====== Audio ======
    AudioEngine& GetAudio() override;
    void PlaySound(const std::string& soundPath, const Vec3* position = nullptr,
                  float volume = 1.0f, bool loop = false) override;

    // ====== Input ======
    InputState GetInput() const override;
    bool IsKeyHeld(KeyCode key) const override;
    void RemapInput(const std::string& actionName, KeyCode key) override;

    // ====== Scripting ======
    ScriptingEngine& GetScripting() override;
    void ExecuteScript(const std::string& scriptPath, const std::string& scriptName = "") override;
    void ReloadScript(const std::string& scriptName) override;

    // ====== Rendering ======
    RenderSystem& GetRender() override;
    Vec3 GetCameraPosition() const override;
    void SetCameraPosition(const Vec3& position) override;

    // ====== Events ======
    IEventSystem& GetEvents() override;

    // ====== Debug ======
    bool IsDeveloperMode() const override;
    void Log(const std::string& message) override;
    void SetDebugRendering(bool enabled) override;

    // ====== Lifecycle ======
    bool ShouldExit() const override;
    void RequestExit() override;
    float GetDeltaTime() const override;
    float GetElapsedTime() const override;

private:
    bool m_shouldExit = false;
    float m_deltaTime = 0.016f;
    float m_elapsedTime = 0.0f;
    bool m_developerMode = true;

    // Pointers to real engine systems (void* to avoid circular includes)
    void* m_realScene = nullptr;
    void* m_realPhysics = nullptr;
    void* m_realAudio = nullptr;
    void* m_realScript = nullptr;
    void* m_camera = nullptr;

    // Dummy systems when real ones not available
    DummyScene m_dummyScene;
    DummyPhysicsWorld m_dummyPhysics;
    DummyAudioEngine m_dummyAudio;
    DummyScriptingEngine m_dummyScript;
    DummyRenderSystem m_dummyRender;

    void InitializeFromConfig(const std::string& configPath);
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================

EngineImpl::EngineImpl() {
    m_developerMode = true;
}

EngineImpl::~EngineImpl() {
    Shutdown();
}

bool EngineImpl::Initialize(const std::string& configPath) {
    Log("=== Engine Initialization ===");
    Log("Config: " + configPath);
    
    InitializeFromConfig(configPath);
    
    // Load prefabs
    GetPrefabManager().LoadPrefabsFromDirectory("assets/prefabs/");
    
    Log("Engine initialized successfully");
    GetEvents().Emit(Events::OnGameStarted);
    return true;
}

void EngineImpl::Shutdown() {
    Log("=== Engine Shutdown ===");
    
    GetHotReloadManager().UnwatchAll();
    GetPrefabManager().UnloadAll();
    ShutdownEventSystem();
    
    Log("Engine shutdown complete");
}

void EngineImpl::InitializeFromConfig(const std::string& configPath) {
    GetConfig().LoadFromFile(configPath);
    
    m_developerMode = GetConfig().GetBool("debug.developerMode", true);
    
    if (m_developerMode) {
        GetConfig().Get<int>("debug.logLevel", 1);
    }
}

// ====== Scene Management ======
void EngineImpl::LoadScene(const std::string& sceneFilePath) {
    Log("Loading scene: " + sceneFilePath);
    // Real implementation would use SceneLoader
}

void EngineImpl::SaveScene(const std::string& sceneFilePath) {
    Log("Saving scene: " + sceneFilePath);
    // Real implementation would use SaveSystem
}

Scene& EngineImpl::GetScene() {
    // TODO: When m_realScene is set, return adapter to real scene
    // For now, return dummy
    return m_dummyScene;
}

entt::registry& EngineImpl::GetRegistry() {
    // The real Scene has: entt::registry Registry
    // Can't access without including Scene.h - needs to be connected in main
    throw std::runtime_error("Scene not initialized - call ConnectEngineSystems()");
}

// ====== Entity Management ======
EntityId EngineImpl::SpawnEntity(const std::string& prefabName, const Vec3& position) {
    Log("Spawning entity: " + prefabName);
    
    if (!GetPrefabManager().HasPrefab(prefabName)) {
        Log("ERROR: Prefab '" + prefabName + "' not found");
        return 0;
    }

    // TODO: Use prefab system to instantiate
    // When real scene is connected:
    //   return GetPrefabManager().Instantiate(prefabName, position, m_realScene);
    
    return 0;
}

void EngineImpl::DestroyEntity(EntityId entityId) {
    Log("Destroying entity: " + std::to_string(entityId));
    // TODO: Cast m_realScene and call DestroyEntity
    GetEvents().Emit(Events::OnEntityDestroyed, &entityId);
}

bool EngineImpl::IsEntityValid(EntityId entityId) const {
    // TODO: Check if entity exists in real scene
    // Requires: Scene* scene = reinterpret_cast<Scene*>(m_realScene);
    //           return scene->Registry.valid(static_cast<entt::entity>(entityId));
    return false;
}

// ====== Transform Queries ======
void EngineImpl::SetPosition(EntityId entityId, const Vec3& position) {
    // TODO: Get entity from m_realScene and set Transform component
}

Vec3 EngineImpl::GetPosition(EntityId entityId) const {
    // TODO: Get entity from m_realScene and read Transform component
    return Vec3(0.0f);
}

void EngineImpl::SetRotation(EntityId entityId, const Quat& rotation) {
    // TODO: Get entity from m_realScene and set Transform rotation
}

Quat EngineImpl::GetRotation(EntityId entityId) const {
    // TODO: Get entity from m_realScene and read Transform rotation
    return Quat(1.0f, 0.0f, 0.0f, 0.0f);
}

// ====== Physics ======
PhysicsWorld& EngineImpl::GetPhysics() {
    // TODO: When m_realPhysics is set, return cast pointer
    return m_dummyPhysics;
}

RaycastHit EngineImpl::Raycast(const Vec3& origin, const Vec3& direction, float maxDistance) {
    // TODO: Use real physics world when connected
    return m_dummyPhysics.Raycast(origin, direction, maxDistance);
}

void EngineImpl::ApplyImpulse(EntityId entityId, const Vec3& impulse) {
    // TODO: Cast m_realPhysics and apply impulse to body
}

// ====== Audio ======
AudioEngine& EngineImpl::GetAudio() {
    // TODO: When m_realAudio is set, return cast pointer
    return m_dummyAudio;
}

void EngineImpl::PlaySound(const std::string& soundPath, const Vec3* position,
                         float volume, bool loop) {
    Log("Playing sound: " + soundPath);
    // TODO: Cast m_realAudio and call PlayOneShot3D
}

// ====== Input ======
InputState EngineImpl::GetInput() const {
    return InputState();
    // Would query input system
}

bool EngineImpl::IsKeyHeld(KeyCode key) const {
    return false;
    // Would query input state
}

void EngineImpl::RemapInput(const std::string& actionName, KeyCode key) {
    Log("Remapping action: " + actionName);
}

// ====== Scripting ======
ScriptingEngine& EngineImpl::GetScripting() {
    // TODO: When m_realScript is set, return cast pointer
    return m_dummyScript;
}

void EngineImpl::ExecuteScript(const std::string& scriptPath, const std::string& scriptName) {
    Log("Executing script: " + (scriptName.empty() ? scriptPath : scriptName));
    // TODO: Cast m_realScript and call RunScript
}

void EngineImpl::ReloadScript(const std::string& scriptName) {
    Log("Reloading script: " + scriptName);
    // TODO: Cast m_realScript and reload
}

// ====== Rendering ======
RenderSystem& EngineImpl::GetRender() {
    return m_dummyRender;
}

Vec3 EngineImpl::GetCameraPosition() const {
    // TODO: If m_camera is set, query it for position
    return Vec3(0.0f, 0.0f, 0.0f);
}

void EngineImpl::SetCameraPosition(const Vec3& position) {
    // TODO: If m_camera is set, update position
}

// ====== Events ======
IEventSystem& EngineImpl::GetEvents() {
    return GetEventSystem();
}

// ====== Debug ======
bool EngineImpl::IsDeveloperMode() const {
    return m_developerMode;
}

void EngineImpl::Log(const std::string& message) {
    // Use std::cout for now - real logging would use Log:: system
    // which requires including core/Log.h (may have dependencies)
    std::cout << "[Engine] " << message << std::endl;
}

void EngineImpl::SetDebugRendering(bool enabled) {
    // Would enable/disable debug visuals
}

// ====== Lifecycle ======
bool EngineImpl::ShouldExit() const {
    return m_shouldExit;
}

void EngineImpl::RequestExit() {
    m_shouldExit = true;
}

float EngineImpl::GetDeltaTime() const {
    return m_deltaTime;
}

float EngineImpl::GetElapsedTime() const {
    return m_elapsedTime;
}

// ============================================================================
// GLOBAL ENGINE INSTANCE
// ============================================================================

static EngineImpl* g_engine = nullptr;

IEngine* GetEngine() {
    return g_engine;
}

IEngine* InitializeEngine(const std::string& configPath) {
    if (g_engine) {
        return g_engine;  // Already initialized
    }

    g_engine = new EngineImpl();
    if (!g_engine->Initialize(configPath)) {
        delete g_engine;
        g_engine = nullptr;
        return nullptr;
    }

    return g_engine;
}

void ShutdownEngine() {
    if (g_engine) {
        g_engine->Shutdown();
        delete g_engine;
        g_engine = nullptr;
    }
}

/**
 * Called from existing main.cpp to connect real systems
 * Usage in main():
 *   IEngine* engine = InitializeEngine("config/engine.yaml");
 *   if (auto* impl = dynamic_cast<EngineImpl*>(engine)) {
 *       impl->SetScene(&scene);
 *       impl->SetPhysicsWorld(&physicsWorld);
 *       impl->SetAudioEngine(&AudioEngine::Get());
 *       impl->SetScriptEngine(&scriptEngine);
 *       impl->SetCamera(&camera);
 *   }
 */
void ConnectEngineSystems(IEngine* engine, void* scene, void* physicsWorld, 
                         void* audioEngine, void* scriptEngine, void* camera) {
    if (auto* impl = dynamic_cast<EngineImpl*>(engine)) {
        if (scene) impl->SetScene(reinterpret_cast<::Scene*>(scene));
        if (physicsWorld) impl->SetPhysicsWorld(reinterpret_cast<::PhysicsWorld*>(physicsWorld));
        if (audioEngine) impl->SetAudioEngine(reinterpret_cast<::AudioEngine*>(audioEngine));
        if (scriptEngine) impl->SetScriptEngine(reinterpret_cast<::ScriptEngine*>(scriptEngine));
        if (camera) impl->SetCamera(reinterpret_cast<::Camera*>(camera));
    }
}
