#pragma once
#include <engine/public/Scene.h>
#include <engine/public/PhysicsWorld.h>
#include <engine/public/AudioEngine.h>
#include <engine/public/ScriptingEngine.h>
#include <engine/public/RenderSystem.h>

#include "../scene/Scene.h"
#include "../scene/Components.h"
#include "../physics/PhysicsWorld.h"
#include "../audio/AudioEngine.h"
#include "../scripting/ScriptEngine.h"
#include "../rendering/Camera.h"

#include <iostream>

// NOTE: Do not add `using namespace VAPublic;` here — VAPublic::Vec3 etc. are
// fine, but several VAPublic interface names (IScene, IPhysicsWorld, IAudioEngine)
// intentionally differ from the real engine class names (Scene, PhysicsWorld,
// AudioEngine) specifically so they can't be accidentally confused. Keep every
// VAPublic type explicitly qualified below.

// ============================================================================
// SceneAdapter
// ============================================================================
class SceneAdapter : public VAPublic::IScene {
public:
    explicit SceneAdapter(::Scene* realScene) : m_scene(realScene) {}

    VAPublic::EntityId CreateEntity() override {
        return static_cast<VAPublic::EntityId>(m_scene->CreateEntity());
    }

    void DestroyEntity(VAPublic::EntityId entityId) override {
        m_scene->DestroyEntity(static_cast<entt::entity>(entityId));
    }

    VAPublic::EntityId FindEntityByName(const std::string&) override {
        std::cout << "[SceneAdapter] FindEntityByName: not supported (no Name component in engine)\n";
        return 0;
    }

    uint32_t GetEntityCount() const override {
        auto view = m_scene->Registry.view<Transform>();
        return static_cast<uint32_t>(view.size());
    }

    void SetSimulationSpeed(float speed) override { m_simSpeed = speed; }
    float GetSimulationSpeed() const override { return m_simSpeed; }
    void Pause() override { m_paused = true; }
    void Resume() override { m_paused = false; }
    bool IsPaused() const override { return m_paused; }

    ::Scene* GetRealScene() { return m_scene; }

private:
    ::Scene* m_scene;
    float m_simSpeed = 1.0f;
    bool m_paused = false;
};

// ============================================================================
// PhysicsWorldAdapter
// ============================================================================
class PhysicsWorldAdapter : public VAPublic::IPhysicsWorld {
public:
    explicit PhysicsWorldAdapter(::PhysicsWorld* realPhysics) : m_physics(realPhysics) {}

    std::vector<VAPublic::RaycastHit> RaycastAll(const VAPublic::Vec3&, const VAPublic::Vec3&, float) override {
        std::cout << "[PhysicsWorldAdapter] RaycastAll: not supported (engine only has closest-hit raycast)\n";
        return {};
    }

    VAPublic::RaycastHit Raycast(const VAPublic::Vec3& origin, const VAPublic::Vec3& direction, float maxDistance) override {
        VAPublic::RaycastHit result;
        glm::vec3 hitPoint;
        if (m_physics->RaycastClosest(origin, direction, maxDistance, hitPoint)) {
            result.hit = true;
            result.hitPoint = hitPoint;
            result.distance = glm::length(hitPoint - origin);
        }
        return result;
    }

    std::vector<VAPublic::EntityId> QuerySphere(const VAPublic::Vec3&, float) override {
        std::cout << "[PhysicsWorldAdapter] QuerySphere: not supported (needs SpatialGrid connection)\n";
        return {};
    }

    std::vector<VAPublic::EntityId> QueryBox(const VAPublic::Vec3&, const VAPublic::Vec3&) override {
        std::cout << "[PhysicsWorldAdapter] QueryBox: not supported (needs SpatialGrid connection)\n";
        return {};
    }

    void SetGravity(const VAPublic::Vec3& gravity) override {
        m_physics->SetGravity(gravity);
        m_lastGravity = gravity;
    }

    VAPublic::Vec3 GetGravity() const override { return m_lastGravity; }

    void Update(float deltaTime) override {
        // CAUTION: main.cpp already calls physicsWorld.Step(deltaTime) once
        // per frame. Do not also call this while that call still exists,
        // or physics will double-step.
        m_physics->Step(deltaTime);
    }

private:
    ::PhysicsWorld* m_physics;
    VAPublic::Vec3 m_lastGravity{0.0f, -9.81f, 0.0f};
};

// ============================================================================
// AudioEngineAdapter
// ============================================================================
class AudioEngineAdapter : public VAPublic::IAudioEngine {
public:
    explicit AudioEngineAdapter(::AudioEngine* realAudio) : m_audio(realAudio) {}

    uint32_t LoadClip(const std::string& filePath) override {
        return static_cast<uint32_t>(m_audio->LoadClip(filePath));
    }

    void UnloadClip(uint32_t) override {
        // Real AudioEngine caches clips for the app's lifetime — no unload path.
    }

    VAPublic::AudioSourceId PlayClip(uint32_t clipId, const VAPublic::Vec3* position,
                           float volume, float, bool loop) override {
        if (position != nullptr) {
            return m_audio->CreateSource3D(static_cast<AudioClipId>(clipId), *position, loop, true, volume);
        }
        return m_audio->CreateSource3D(static_cast<AudioClipId>(clipId), VAPublic::Vec3(0.0f), loop, true, volume);
    }

    void Stop(VAPublic::AudioSourceId sourceId) override {
        m_audio->StopSource(static_cast<AudioSourceHandle>(sourceId));
    }

    void Pause(VAPublic::AudioSourceId) override {
        std::cout << "[AudioEngineAdapter] Pause: not supported\n";
    }

    void Resume(VAPublic::AudioSourceId sourceId) override {
        m_audio->PlaySource(static_cast<AudioSourceHandle>(sourceId));
    }

    bool IsPlaying(VAPublic::AudioSourceId) override {
        return false;
    }

    void SetVolume(VAPublic::AudioSourceId sourceId, float volume) override {
        m_audio->SetSourceVolume(static_cast<AudioSourceHandle>(sourceId), volume);
    }

    void SetPosition(VAPublic::AudioSourceId sourceId, const VAPublic::Vec3& position) override {
        m_audio->SetSourcePosition(static_cast<AudioSourceHandle>(sourceId), position);
    }

    void SetListenerTransform(const VAPublic::Vec3& position, const VAPublic::Vec3& forward, const VAPublic::Vec3& up) override {
        m_audio->SetListener(position, forward, up);
    }

    void Update(float) override {
        m_audio->Update();
    }

private:
    ::AudioEngine* m_audio;
};

// ============================================================================
// ScriptingEngineAdapter
// ============================================================================
class ScriptingEngineAdapter : public VAPublic::IScriptingEngine {
public:
    explicit ScriptingEngineAdapter(::ScriptEngine* realScript) : m_script(realScript) {}

    void ExecuteScript(const std::string& scriptPath, const std::string&) override {
        m_script->RunScript(scriptPath);
    }

    void ExecuteCode(const std::string&) override {
        std::cout << "[ScriptingEngineAdapter] ExecuteCode: not supported (engine only runs script files)\n";
    }

    void ReloadScript(const std::string&) override {
        std::cout << "[ScriptingEngineAdapter] ReloadScript: not directly supported (engine auto-reloads on file change)\n";
    }

    void SetGlobal(const std::string&, const void*) override {
        std::cout << "[ScriptingEngineAdapter] SetGlobal: not supported (no generic binding path)\n";
    }

    sol::state* GetLuaState() override {
        return nullptr; // ScriptEngine's sol::state is private with no accessor
    }

    void CallFunction(const std::string&) override {
        std::cout << "[ScriptingEngineAdapter] CallFunction: not supported (only on_update/on_load are exposed)\n";
    }

    bool HasFunction(const std::string&) override {
        return false;
    }

private:
    ::ScriptEngine* m_script;
};

// ============================================================================
// RenderSystemAdapter
// ============================================================================
class RenderSystemAdapter : public VAPublic::IRenderSystem {
public:
    explicit RenderSystemAdapter(::Camera* realCamera) : m_camera(realCamera) {}

    VAPublic::Vec3 GetCameraPosition() const override { return m_camera->Position; }
    void SetCameraPosition(const VAPublic::Vec3& position) override { m_camera->Position = position; }
    VAPublic::Vec3 GetCameraForward() const override { return m_camera->GetFront(); }

    VAPublic::Vec3 GetCameraRight() const override {
        return glm::normalize(glm::cross(m_camera->GetFront(), glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    VAPublic::Vec3 GetCameraUp() const override {
        return glm::normalize(glm::cross(GetCameraRight(), m_camera->GetFront()));
    }

    void SetCameraLookAt(const VAPublic::Vec3&, const VAPublic::Vec3&, const VAPublic::Vec3&) override {
        std::cout << "[RenderSystemAdapter] SetCameraLookAt: not supported (camera is yaw/pitch driven)\n";
    }

    void SetDebugRendering(bool) override {
        std::cout << "[RenderSystemAdapter] SetDebugRendering: not wired to any real toggle yet\n";
    }
    bool IsDebugRenderingEnabled() const override { return false; }

    int GetViewportWidth() const override { return m_viewportWidth; }
    int GetViewportHeight() const override { return m_viewportHeight; }
    void SetViewportSize(int w, int h) { m_viewportWidth = w; m_viewportHeight = h; }

    uint32_t LoadTexture(const std::string&) override {
        std::cout << "[RenderSystemAdapter] LoadTexture: not implemented (needs texture-id registry)\n";
        return 0;
    }
    uint32_t LoadModel(const std::string&) override {
        std::cout << "[RenderSystemAdapter] LoadModel: not implemented (needs model-id registry)\n";
        return 0;
    }

private:
    ::Camera* m_camera;
    int m_viewportWidth = 1920;
    int m_viewportHeight = 1080;
};