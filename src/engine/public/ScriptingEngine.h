#pragma once

#include <string>
#include <memory>

// Forward declarations
namespace sol { class state; }

// ============================================================================
// SCRIPTING ENGINE INTERFACE
// ============================================================================

/**
 * Lua scripting system
 */
class ScriptingEngine {
public:
    virtual ~ScriptingEngine() = default;

    /**
     * Execute a Lua script file
     * @param filePath Path to .lua file
     * @param scriptName Optional name for debugging
     */
    virtual void ExecuteScript(const std::string& filePath, const std::string& scriptName = "") = 0;

    /**
     * Execute a Lua string directly
     * @param code Lua code to execute
     */
    virtual void ExecuteCode(const std::string& code) = 0;

    /**
     * Reload a script (hot-reload)
     * @param scriptName Name of script to reload
     */
    virtual void ReloadScript(const std::string& scriptName) = 0;

    /**
     * Register a C++ function to be called from Lua
     * Template version - use like:
     *   RegisterFunction("MyFunc", [](int x) { return x * 2; });
     */
    template<typename Func>
    void RegisterFunction(const std::string& name, Func func);

    /**
     * Set a global variable in Lua
     * @param name Variable name
     * @param value Value (as void* - cast from pointer types)
     */
    virtual void SetGlobal(const std::string& name, const void* value) = 0;

    /**
     * Get the underlying Lua state for advanced usage
     * @return sol::state pointer (C++)
     */
    virtual sol::state* GetLuaState() = 0;

    /**
     * Call a Lua function by name
     * @param functionName Name of function
     */
    virtual void CallFunction(const std::string& functionName) = 0;

    /**
     * Check if a function exists
     * @param functionName Name of function
     * @return true if function is defined
     */
    virtual bool HasFunction(const std::string& functionName) = 0;
};
