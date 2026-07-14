#pragma once
#include <sol/sol.hpp>
#include <string>
#include "../scene/Scene.h"
#include <filesystem>

class ScriptEngine {
public:
    ScriptEngine();

    void Initialize(Scene* scene);
    void RunScript(const std::string& path);

    void CallUpdate(float deltaTime);

    void CheckForReload(float deltaTime);

private:
    void BindEngineAPI();

    sol::state m_lua;
    Scene* m_scene = nullptr;

    std::string m_currentScriptPath;
    std::filesystem::file_time_type m_lastWriteTime;
    float m_reloadCheckTimer = 0.0f;
};