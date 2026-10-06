#include <engine/public/EngineAPI.h>
#include <engine/public/EventSystem.h>
#include <engine/Config.h>
#include <engine/Prefab.h>
#include <engine/HotReload.h>
#include "Adapters.h"
#include <iostream>
#include <stdexcept>
#include <memory>

namespace VAPublic {

extern IEventSystem& GetEventSystem();
extern void ShutdownEventSystem();

// Dummy fallbacks — used only when a system hasn't been connected yet.
class DummyScene : public IScene {
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

class DummyPhysicsWorld : public IPhysicsWorld {
public:
    std::vector<RaycastHit> RaycastAll(const Vec3&, const Vec3&, float) override { return {}; }
    RaycastHit Raycast(const Vec3&, const Vec3&, float) override { return RaycastHit{}; }
    std::vector<EntityId> QuerySphere(const Vec3&, float) override { return {}; }
    std::vector<EntityId> QueryBox(const Vec3&, const Vec3&) override { return {}; }
    void SetGravity(const Vec3&) override {}
    Vec3 GetGravity() const override { return Vec3(0, -9.81f, 0); }
    void Update(float) override {}
};

class DummyAudioEngine : public IAudioEngine {
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

class DummyScriptingEngine : public IScriptingEngine {
public:
    void ExecuteScript(const std::string&, const std::string& = "") override {}
    void ExecuteCode(const std::string&) override {}
    void ReloadScript(const std::string&) override {}
    void SetGlobal(const std::string&, const void*) override {}
    sol::state* GetLuaState() override { return nullptr; }
    void CallFunction(const std::string&) override {}
    bool HasFunction(const std::string&) override { return false; }
};

class DummyRenderSystem : public IRenderSystem {
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

class EngineImpl : public IEngine {
public:
    EngineImpl() = default;
    ~EngineImpl() override { Shutdown(); }

    bool Initialize(const std::string& configPath);
    void Shutdown();

    void SetScene(::Scene* scene) { m_sceneAdapter = std::make_unique<SceneAdapter>(scene); }
    void SetPhysicsWorld(::PhysicsWorld* physics) { m_physicsAdapter = std::make_unique<PhysicsWorldAdapter>(physics); }
    void SetAudioEngine(::AudioEngine* audio) { m_audioAdapter = std::make_unique<AudioEngineAdapter>(audio); }
    void SetScriptEngine(::ScriptEngine* script) { m_scriptAdapter = std::make_unique<ScriptingEngineAdapter>(script); }
    void SetCamera(::Camera* camera) { m_renderAdapter = std::make_unique<RenderSystemAdapter>(camera); }

    void LoadScene(const std::string& sceneFilePath) override;
    void SaveScene(const std::string& sceneFilePath) override;
    IScene& GetScene() override { return m_sceneAdapter ? static_cast<IScene&>(*m_sceneAdapter) : static_cast<IScene&>(m_dummyScene); }
    entt::registry& GetRegistry() override {
        if (!m_sceneAdapter) throw std::runtime_error("Scene not initialized - call ConnectEngineSystems()");
        return m_sceneAdapter->GetRealScene()->Registry;
    }

    EntityId SpawnEntity(const std::string& prefabName, const Vec3& position) override;
    void DestroyEntity(EntityId entityId) override;
    bool IsEntityValid(EntityId entityId) const override;

    void SetPosition(EntityId entityId, const Vec3& position) override;
    Vec3 GetPosition(EntityId entityId) const override;
    void SetRotation(EntityId entityId, const Quat& rotation) override;
    Quat GetRotation(EntityId entityId) const override;

    IPhysicsWorld& GetPhysics() override { return m_physicsAdapter ? static_cast<IPhysicsWorld&>(*m_physicsAdapter) : static_cast<IPhysicsWorld&>(m_dummyPhysics); }
    RaycastHit Raycast(const Vec3& origin, const Vec3& direction, float maxDistance) override { return GetPhysics().Raycast(origin, direction, maxDistance); }
    void ApplyImpulse(EntityId, const Vec3&) override {
        std::cout << "[Engine] ApplyImpulse: not supported (real PhysicsWorld has no impulse API yet)\n";
    }

    IAudioEngine& GetAudio() override { return m_audioAdapter ? static_cast<IAudioEngine&>(*m_audioAdapter) : static_cast<IAudioEngine&>(m_dummyAudio); }
    void PlaySound(const std::string& soundPath, const Vec3* position, float volume, bool loop) override {
        uint32_t clip = GetAudio().LoadClip(soundPath);
        GetAudio().PlayClip(clip, position, volume, 1.0f, loop);
    }

    InputState GetInput() const override { return InputState{}; }
    bool IsKeyHeld(KeyCode) const override { return false; }
    void RemapInput(const std::string&, KeyCode) override {}

    IScriptingEngine& GetScripting() override { return m_scriptAdapter ? static_cast<IScriptingEngine&>(*m_scriptAdapter) : static_cast<IScriptingEngine&>(m_dummyScript); }
    void ExecuteScript(const std::string& scriptPath, const std::string& scriptName) override { GetScripting().ExecuteScript(scriptPath, scriptName); }
    void ReloadScript(const std::string& scriptName) override { GetScripting().ReloadScript(scriptName); }

    IRenderSystem& GetRender() override { return m_renderAdapter ? static_cast<IRenderSystem&>(*m_renderAdapter) : static_cast<IRenderSystem&>(m_dummyRender); }
    Vec3 GetCameraPosition() const override { return const_cast<EngineImpl*>(this)->GetRender().GetCameraPosition(); }
    void SetCameraPosition(const Vec3& position) override { GetRender().SetCameraPosition(position); }

    IEventSystem& GetEvents() override { return GetEventSystem(); }

    bool IsDeveloperMode() const override { return m_developerMode; }
    void Log(const std::string& message) override { std::cout << "[Engine] " << message << std::endl; }
    void SetDebugRendering(bool enabled) override { GetRender().SetDebugRendering(enabled); }

    bool ShouldExit() const override { return m_shouldExit; }
    void RequestExit() override { m_shouldExit = true; }
    float GetDeltaTime() const override { return m_deltaTime; }
    float GetElapsedTime() const override { return m_elapsedTime; }

private:
    bool m_shouldExit = false;
    float m_deltaTime = 0.016f;
    float m_elapsedTime = 0.0f;
    bool m_developerMode = true;

    std::unique_ptr<SceneAdapter> m_sceneAdapter;
    std::unique_ptr<PhysicsWorldAdapter> m_physicsAdapter;
    std::unique_ptr<AudioEngineAdapter> m_audioAdapter;
    std::unique_ptr<ScriptingEngineAdapter> m_scriptAdapter;
    std::unique_ptr<RenderSystemAdapter> m_renderAdapter;

    DummyScene m_dummyScene;
    DummyPhysicsWorld m_dummyPhysics;
    DummyAudioEngine m_dummyAudio;
    DummyScriptingEngine m_dummyScript;
    DummyRenderSystem m_dummyRender;

    void InitializeFromConfig(const std::string& configPath);
};

bool EngineImpl::Initialize(const std::string& configPath) {
    Log("=== Engine Initialization ===");
    Log("Config: " + configPath);
    InitializeFromConfig(configPath);
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
}

void EngineImpl::LoadScene(const std::string& sceneFilePath) {
    Log("LoadScene: not wired to SceneLoader yet — call SceneLoader::LoadFromFile directly for now: " + sceneFilePath);
}

void EngineImpl::SaveScene(const std::string& sceneFilePath) {
    Log("SaveScene: not wired to SaveSystem yet — call SaveSystem::WriteSaveFile directly for now: " + sceneFilePath);
}

EntityId EngineImpl::SpawnEntity(const std::string& prefabName, const Vec3& position) {
    if (!GetPrefabManager().HasPrefab(prefabName)) {
        Log("ERROR: Prefab '" + prefabName + "' not found");
        return 0;
    }
    if (!m_sceneAdapter) {
        Log("ERROR: SpawnEntity called before scene connected");
        return 0;
    }

    ::PhysicsWorld* realPhysics = m_physicsAdapter ? m_physicsAdapter->GetRealPhysics() : nullptr;
    ::ScriptEngine* realScript = m_scriptAdapter ? m_scriptAdapter->GetRealScriptEngine() : nullptr;

    return GetPrefabManager().Instantiate(
        prefabName, position, m_sceneAdapter->GetRealScene(), realPhysics, realScript);
}

void EngineImpl::DestroyEntity(EntityId entityId) {
    GetScene().DestroyEntity(entityId);
    GetEvents().Emit(Events::OnEntityDestroyed, &entityId);
}

bool EngineImpl::IsEntityValid(EntityId entityId) const {
    if (!m_sceneAdapter) return false;
    return m_sceneAdapter->GetRealScene()->Registry.valid(static_cast<entt::entity>(entityId));
}

void EngineImpl::SetPosition(EntityId entityId, const Vec3& position) {
    if (!m_sceneAdapter) return;
    auto& registry = m_sceneAdapter->GetRealScene()->Registry;
    auto entity = static_cast<entt::entity>(entityId);
    if (registry.valid(entity) && registry.all_of<::Transform>(entity)) {
        registry.get<::Transform>(entity).Position = position;
    }
}

Vec3 EngineImpl::GetPosition(EntityId entityId) const {
    if (!m_sceneAdapter) return Vec3(0.0f);
    auto& registry = m_sceneAdapter->GetRealScene()->Registry;
    auto entity = static_cast<entt::entity>(entityId);
    if (registry.valid(entity) && registry.all_of<::Transform>(entity)) {
        return registry.get<::Transform>(entity).Position;
    }
    return Vec3(0.0f);
}

void EngineImpl::SetRotation(EntityId entityId, const Quat& rotation) {
    if (!m_sceneAdapter) return;
    auto& registry = m_sceneAdapter->GetRealScene()->Registry;
    auto entity = static_cast<entt::entity>(entityId);
    if (registry.valid(entity) && registry.all_of<::Transform>(entity)) {
        registry.get<::Transform>(entity).Rotation = rotation;
    }
}

Quat EngineImpl::GetRotation(EntityId entityId) const {
    if (!m_sceneAdapter) return Quat(1.0f, 0.0f, 0.0f, 0.0f);
    auto& registry = m_sceneAdapter->GetRealScene()->Registry;
    auto entity = static_cast<entt::entity>(entityId);
    if (registry.valid(entity) && registry.all_of<::Transform>(entity)) {
        return registry.get<::Transform>(entity).Rotation;
    }
    return Quat(1.0f, 0.0f, 0.0f, 0.0f);
}

static EngineImpl* g_engine = nullptr;

IEngine* GetEngine() { return g_engine; }

IEngine* InitializeEngine(const std::string& configPath) {
    if (g_engine) return g_engine;
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

} // namespace VAPublic