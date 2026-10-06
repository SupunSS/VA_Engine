#pragma once

#include <string>
#include <memory>

// Forward declarations
namespace sol { class state; }

namespace VAPublic {

// ============================================================================
// SCRIPTING ENGINE INTERFACE
// ============================================================================

class IScriptingEngine {
public:
    virtual ~IScriptingEngine() = default;

    virtual void ExecuteScript(const std::string& filePath, const std::string& scriptName = "") = 0;
    virtual void ExecuteCode(const std::string& code) = 0;
    virtual void ReloadScript(const std::string& scriptName) = 0;

    template<typename Func>
    void RegisterFunction(const std::string& name, Func func);

    virtual void SetGlobal(const std::string& name, const void* value) = 0;
    virtual sol::state* GetLuaState() = 0;
    virtual void CallFunction(const std::string& functionName) = 0;
    virtual bool HasFunction(const std::string& functionName) = 0;
};

} // namespace VAPublic