#include "ScriptEngine.h"
#include "../core/Log.h"
#include "../scene/Components.h"
#include "../rendering/Model.h"
#include <unordered_map>
#include <filesystem>
#include "../core/Assert.h"

ScriptEngine::ScriptEngine() {
    m_lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string);
}

void ScriptEngine::Initialize(Scene* scene) {
    m_scene = scene;
    BindEngineAPI();
}

void ScriptEngine::BindEngineAPI() {
    // Logging — scripts can print through the engine's logger
    m_lua.set_function("log_info", [](const std::string& msg) {
        Log::Info("[Lua] {}", msg);
    });

    // Expose glm::vec3 as a usable Lua type
    m_lua.new_usertype<glm::vec3>("Vec3",
        sol::constructors<glm::vec3(), glm::vec3(float, float, float)>(),
        "x", &glm::vec3::x,
        "y", &glm::vec3::y,
        "z", &glm::vec3::z
    );

    // Expose Transform component
    m_lua.new_usertype<Transform>("Transform",
        "position", &Transform::Position
    );

    // Entity API — scripts work with raw entity handles (as integers)
    // and call back into the engine to manipulate them
    m_lua.set_function("get_transform", [this](uint32_t entityId) -> Transform& {
        entt::entity entity = static_cast<entt::entity>(entityId);
        ENGINE_ASSERT(m_scene->Registry.valid(entity), "Invalid entity passed to get_transform");
        return m_scene->Registry.get<Transform>(entity);
    });

    m_lua.set_function("create_entity", [this]() -> uint32_t {
        return static_cast<uint32_t>(m_scene->CreateEntity());
    });

    m_lua.set_function("spawn_model", [this](const std::string& modelPath, float x, float y, float z) -> uint32_t {
    static std::unordered_map<std::string, std::shared_ptr<Model>> scriptModelCache;

    if (scriptModelCache.find(modelPath) == scriptModelCache.end()) {
        scriptModelCache[modelPath] = std::make_shared<Model>(modelPath);
    }

    auto entity = m_scene->CreateEntity();
    m_scene->Registry.get<Transform>(entity).Position = glm::vec3(x, y, z);
    m_scene->Registry.emplace<MeshRenderer>(entity, scriptModelCache[modelPath]);

    return static_cast<uint32_t>(entity);
});
}

void ScriptEngine::RunScript(const std::string& path) {
    m_currentScriptPath = path;
    m_lastWriteTime = std::filesystem::last_write_time(path);

    auto result = m_lua.safe_script_file(path, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        Log::Error("Lua script error: {}", err.what());
        return; // Non-fatal — a broken script should log and stop, not
                // crash the whole engine.
    }

    // Run one-time setup after the script's functions are defined
    sol::function loadFn = m_lua["on_load"];
    if (loadFn.valid()) {
        auto loadResult = loadFn();
        if (!loadResult.valid()) {
            sol::error err = loadResult;
            Log::Error("Lua on_load error: {}", err.what());
        }
    }
}

void ScriptEngine::CheckForReload(float deltaTime) {
    m_reloadCheckTimer += deltaTime;
    if (m_reloadCheckTimer < 0.5f) return;
    m_reloadCheckTimer = 0.0f;

    auto currentWriteTime = std::filesystem::last_write_time(m_currentScriptPath);
    if (currentWriteTime != m_lastWriteTime) {
        Log::Info("Detected change in {}, reloading...", m_currentScriptPath);
        m_lastWriteTime = currentWriteTime;

        auto result = m_lua.safe_script_file(m_currentScriptPath, sol::script_pass_on_error);
        if (!result.valid()) {
            sol::error err = result;
            Log::Error("Lua reload error: {}", err.what());
        }
    }
}

void ScriptEngine::CallUpdate(float deltaTime) {
    sol::function updateFn = m_lua["on_update"];
    if (updateFn.valid()) {
        auto result = updateFn(deltaTime);
        if (!result.valid()) {
            sol::error err = result;
            Log::Error("Lua on_update error: {}", err.what());
        }
    }
}

// ============================================================================
// Per-entity scripting
// ============================================================================

void ScriptEngine::AttachScript(entt::entity entity, const std::string& path) {
    // Isolated environment per entity: sol::environment creates a fresh
    // table used as the script's globals, but falls back to the real
    // global table (m_lua's globals — where RegisterLuaBindings put
    // `Engine`, `KeyCode`, etc.) for anything not found locally. This is
    // what lets two entities run the identical script file without one's
    // `self.someState = ...` clobbering the other's.
    sol::environment env(m_lua, sol::create, m_lua.globals());
    env["entity_id"] = static_cast<uint32_t>(entity);

    auto result = m_lua.safe_script_file(path, env, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        Log::Error("Lua entity-script error ({}): {}", path, err.what());
        return; // Don't attach a script that failed to even load once.
    }

    m_entityScripts[entity] = EntityScript{ path, std::move(env) };

    sol::function loadFn = m_entityScripts[entity].Env["on_load"];
    if (loadFn.valid()) {
        auto loadResult = loadFn();
        if (!loadResult.valid()) {
            sol::error err = loadResult;
            Log::Error("Lua entity-script on_load error ({}): {}", path, err.what());
        }
    }

    if (m_scene && m_scene->Registry.valid(entity)) {
        m_scene->Registry.emplace_or_replace<ScriptComponent>(entity, ScriptComponent{ path });
    }
}

void ScriptEngine::DetachScript(entt::entity entity) {
    m_entityScripts.erase(entity);
    if (m_scene && m_scene->Registry.valid(entity) && m_scene->Registry.all_of<ScriptComponent>(entity)) {
        m_scene->Registry.remove<ScriptComponent>(entity);
    }
}

bool ScriptEngine::HasScript(entt::entity entity) const {
    return m_entityScripts.find(entity) != m_entityScripts.end();
}

std::string ScriptEngine::GetAttachedScriptPath(entt::entity entity) const {
    auto it = m_entityScripts.find(entity);
    return it != m_entityScripts.end() ? it->second.Path : std::string{};
}

void ScriptEngine::CallEntityUpdates(float deltaTime) {
    // Copy entity list first: an on_update could itself call
    // DetachScript/AttachScript (e.g. a script that removes itself),
    // which would invalidate the map's iterators mid-loop otherwise.
    std::vector<entt::entity> entities;
    entities.reserve(m_entityScripts.size());
    for (const auto& [entity, script] : m_entityScripts) {
        entities.push_back(entity);
    }

    for (auto entity : entities) {
        auto it = m_entityScripts.find(entity);
        if (it == m_entityScripts.end()) continue; // detached mid-loop by an earlier script this same frame

        sol::function updateFn = it->second.Env["on_update"];
        if (!updateFn.valid()) continue;

        auto result = updateFn(deltaTime);
        if (!result.valid()) {
            sol::error err = result;
            Log::Error("Lua entity-script on_update error ({}): {}", it->second.Path, err.what());
        }
    }
}