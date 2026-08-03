#pragma once
#include <sol/sol.hpp>
#include <string>
#include <unordered_map>
#include "../scene/Scene.h"
#include <filesystem>
#include <entt/entt.hpp>

class ScriptEngine {
public:
    ScriptEngine();

    void Initialize(Scene* scene);
    void RunScript(const std::string& path);

    void CallUpdate(float deltaTime);

    void CheckForReload(float deltaTime);

    sol::state& GetLuaState() { return m_lua; }

    // --- Per-entity scripting -------------------------------------------
    // Attaches (or replaces) a script on a specific entity, executed in
    // its own isolated sol::environment — so two entities running the
    // exact same script file each get their own private `self`/globals,
    // rather than silently sharing one global table the way RunScript()'s
    // main script does. The shared `Engine` API table (from
    // RegisterLuaBindings) is still reachable from inside an entity
    // script's environment via sol's normal fallback to the real globals.
    //
    // Inside an attached script, `entity_id` is available as a global
    // integer giving the script access to its own entity id (e.g. to call
    // Engine:GetPosition(entity_id), Engine:SetPosition(entity_id, ...)).
    // An `on_update(deltaTime)` function, if defined, is called once per
    // frame via CallEntityUpdates().
    void AttachScript(entt::entity entity, const std::string& path);
    void DetachScript(entt::entity entity);
    bool HasScript(entt::entity entity) const;
    std::string GetAttachedScriptPath(entt::entity entity) const;

    // Runs on_update(deltaTime) for every entity with an active attached
    // script environment. Call once per frame, alongside CallUpdate().
    void CallEntityUpdates(float deltaTime);

private:
    void BindEngineAPI();

    sol::state m_lua;
    Scene* m_scene = nullptr;

    std::string m_currentScriptPath;
    std::filesystem::file_time_type m_lastWriteTime;
    float m_reloadCheckTimer = 0.0f;

    struct EntityScript {
        std::string Path;
        sol::environment Env;
    };
    std::unordered_map<entt::entity, EntityScript> m_entityScripts;
};